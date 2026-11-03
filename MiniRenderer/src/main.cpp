// 3단계: 첫 삼각형
// 목표: 정점 데이터를 GPU로 보내고, 셰이더를 거쳐 화면에 색이 있는 삼각형을 그린다.

#include <cstddef>        // offsetof
#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>      // 창, 메시지, OutputDebugStringA 등 Windows 기본 기능
#include <d3d11.h>        // Direct3D 11
#include <d3dcompiler.h>  // D3DCompileFromFile: HLSL 셰이더를 실행 중에 컴파일
#include <wrl/client.h>   // ComPtr: DirectX 객체를 자동으로 Release 해 주는 스마트 포인터

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------
// 윈도우 프로시저: Windows가 보내는 "쪽지(메시지)"를 받아 처리하는 함수.
// 우리가 직접 부르지 않는다. Windows가 필요할 때 이 함수를 호출한다.
// ---------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        // 사용자가 X 버튼을 눌렀다. 창을 파괴한다.
        std::printf("[메시지] WM_CLOSE   : X 버튼 클릭 -> 창을 파괴합니다\n");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        // 창이 사라졌다. 메시지 루프에 "이제 끝내자"(WM_QUIT)를 보낸다.
        std::printf("[메시지] WM_DESTROY : 창이 사라짐 -> 종료를 요청합니다\n");
        PostQuitMessage(0);
        return 0;
    }

    // 우리가 처리하지 않는 나머지 쪽지(창 이동, 크기 조절 등)는 Windows 기본 처리에 맡긴다.
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------
// D3D11 객체들 (9단계에서 Renderer로 옮기기 전까지는 전역으로 둔다)
// ---------------------------------------------------------------
ComPtr<ID3D11Device>           g_device;            // 만드는 창구: 버퍼, 텍스처 등 GPU 자원 생성
ComPtr<ID3D11DeviceContext>    g_context;           // 시키는 창구: 지우기, 그리기 등 명령 전달
ComPtr<IDXGISwapChain>         g_swapChain;         // 백 버퍼/프런트 버퍼 묶음
ComPtr<ID3D11RenderTargetView> g_renderTargetView;  // "이 백 버퍼에 그려라" 이름표
ComPtr<ID3D11InfoQueue>        g_infoQueue;         // 디버그 레이어의 메시지 보관함 (디버그 레이어가 켜졌을 때만)

// 3단계: 삼각형 그리기에 필요한 객체들
ComPtr<ID3D11Buffer>           g_vertexBuffer;      // 정점 데이터를 담은 GPU 메모리
ComPtr<ID3D11VertexShader>     g_vertexShader;      // 정점마다 실행되는 GPU 프로그램
ComPtr<ID3D11PixelShader>      g_pixelShader;       // 픽셀마다 실행되는 GPU 프로그램
ComPtr<ID3D11InputLayout>      g_inputLayout;       // 정점 버퍼의 바이트를 읽는 법(설명서)

// 정점 하나의 모양. 셰이더의 VSInput 과 순서·크기가 맞아야 한다.
struct Vertex
{
    float position[3];   // x, y, z            -> 12바이트
    float color[4];      // 빨강, 초록, 파랑, 투명도 -> 16바이트
};

// 실패한 단계 이름과 HRESULT 값을 출력한다.
void PrintHResultError(const char* step, HRESULT hr)
{
    std::printf("[오류] %s 실패 (HRESULT 0x%08lX)\n", step, static_cast<unsigned long>(hr));
}

// 디버그 레이어가 쌓아 둔 메시지를 꺼내 콘솔에 출력한다.
// (원래는 디버거 출력 창에만 보이는 메시지를 터미널에서도 보기 위함)
void PrintD3DDebugMessages()
{
    if (!g_infoQueue)
    {
        return;   // 디버그 레이어가 꺼져 있으면 보관함도 없다.
    }

    const UINT64 count = g_infoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < count; ++i)
    {
        // 메시지 크기를 먼저 물어보고, 그만큼 메모리를 준비한 뒤 내용을 받는다.
        SIZE_T size = 0;
        g_infoQueue->GetMessage(i, nullptr, &size);
        std::vector<char> buffer(size);
        D3D11_MESSAGE* message = reinterpret_cast<D3D11_MESSAGE*>(buffer.data());
        g_infoQueue->GetMessage(i, message, &size);

        // 정보성 메시지는 건너뛰고 문제가 있는 것만 출력한다.
        const char* level = nullptr;
        switch (message->Severity)
        {
        case D3D11_MESSAGE_SEVERITY_CORRUPTION: level = "CORRUPTION"; break;
        case D3D11_MESSAGE_SEVERITY_ERROR:      level = "ERROR";      break;
        case D3D11_MESSAGE_SEVERITY_WARNING:    level = "WARNING";    break;
        default: break;
        }
        if (level)
        {
            std::printf("[D3D11 %s] %s\n", level, message->pDescription);
        }
    }
    g_infoQueue->ClearStoredMessages();
}

bool InitD3D(HWND hwnd, int width, int height)
{
    // ---------------------------------------------------------------
    // 1) 스왑 체인 설명서: 어떤 그림판을 몇 장, 어느 창에 쓸지
    // ---------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferDesc.Width  = width;                          // 백 버퍼 크기 = 클라이언트 영역 크기
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // 픽셀 하나 = 빨강/초록/파랑/투명도 각 8비트
    scd.SampleDesc.Count  = 1;                              // 안티앨리어싱 없음
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 용도: 그림을 그릴 대상
    scd.BufferCount       = 2;                              // 그림판 2장
    scd.OutputWindow      = hwnd;                           // 1단계에서 만든 창의 번호표
    scd.Windowed          = TRUE;                           // 전체 화면이 아닌 창 모드
    scd.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // 맞바꾸기 방식 (현대 Windows 권장)

    // ---------------------------------------------------------------
    // 2) 디바이스 + 디바이스 컨텍스트 + 스왑 체인을 한 번에 생성
    //    먼저 디버그 레이어를 켜서 시도하고, 설치되어 있지 않으면 끄고 다시 시도한다.
    // ---------------------------------------------------------------
    UINT flags = D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel = {};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // 기본 그래픽 카드 사용
        D3D_DRIVER_TYPE_HARDWARE,   // 실제 GPU 사용
        nullptr, flags,
        nullptr, 0,                 // 지원 가능한 가장 높은 기능 수준을 자동 선택
        D3D11_SDK_VERSION,
        &scd, &g_swapChain, &g_device, &featureLevel, &g_context);

    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        std::printf("[안내] 디버그 레이어가 없어 끄고 진행합니다 (그래픽 도구 미설치)\n");
        flags = 0;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION,
            &scd, &g_swapChain, &g_device, &featureLevel, &g_context);
    }
    if (FAILED(hr))
    {
        PrintHResultError("디바이스/스왑 체인 생성", hr);
        return false;
    }
    std::printf("D3D11 디바이스 생성 완료 (기능 수준 0x%X, 디버그 레이어 %s)\n",
                static_cast<unsigned>(featureLevel), flags ? "켜짐" : "꺼짐");

    // 디버그 레이어가 켜졌다면, 디바이스에게 "메시지 보관함" 인터페이스를 달라고 한다.
    if (flags & D3D11_CREATE_DEVICE_DEBUG)
    {
        g_device.As(&g_infoQueue);
    }

    // ---------------------------------------------------------------
    // 3) 백 버퍼를 꺼내 렌더 타깃 뷰(이름표)를 붙인다.
    // ---------------------------------------------------------------
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = g_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr))
    {
        PrintHResultError("백 버퍼 가져오기", hr);
        return false;
    }

    hr = g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_renderTargetView);
    if (FAILED(hr))
    {
        PrintHResultError("렌더 타깃 뷰 생성", hr);
        return false;
    }
    // backBuffer는 이 함수가 끝나면 자동 Release 된다.
    // 백 버퍼 자체는 스왑 체인과 렌더 타깃 뷰가 계속 붙잡고 있으므로 사라지지 않는다.

    return true;
}

// 셰이더 파일의 전체 경로를 만든다.
// 실행 파일(build\MiniRenderer.exe) 위치를 기준으로 ..\shaders\ 폴더를 찾으므로
// 어느 폴더에서 실행해도 같은 파일을 읽는다.
std::wstring GetShaderPath(const wchar_t* fileName)
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring path = exePath;
    path = path.substr(0, path.find_last_of(L"\\/") + 1);   // "...\build\" 까지만 남김
    return path + L"..\\shaders\\" + fileName;
}

// HLSL 파일 안의 함수(entryPoint) 하나를 GPU용 코드로 컴파일한다.
// target: "vs_5_0" = 정점 셰이더(셰이더 모델 5.0), "ps_5_0" = 픽셀 셰이더
ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entryPoint, const char* target)
{
    ComPtr<ID3DBlob> code;     // 컴파일 결과 (GPU가 이해하는 바이트 코드)
    ComPtr<ID3DBlob> errors;   // 실패했을 때의 오류 문장

    HRESULT hr = D3DCompileFromFile(
        path.c_str(), nullptr, nullptr,
        entryPoint, target,
        D3DCOMPILE_DEBUG,      // 디버깅하기 쉬운 형태로 컴파일
        0, &code, &errors);

    if (FAILED(hr))
    {
        if (errors)
        {
            // HLSL 문법 오류: 파일 이름, 줄 번호, 오류 내용이 들어 있다.
            std::printf("[셰이더 오류] %s\n", static_cast<const char*>(errors->GetBufferPointer()));
        }
        else
        {
            // 오류 문장이 없다면 대개 파일을 못 찾은 경우다.
            std::printf("[오류] 셰이더 파일을 열 수 없음: %ls\n", path.c_str());
            PrintHResultError("셰이더 컴파일", hr);
        }
        return nullptr;
    }
    return code;
}

bool InitTriangle()
{
    // ---------------------------------------------------------------
    // 1) 정점 데이터 (아직 CPU 메모리에 있는 평범한 C++ 배열)
    //    좌표는 화면 가운데가 (0,0), 왼쪽/오른쪽 끝이 -1/+1, 아래/위 끝이 -1/+1.
    //    위 -> 오른쪽 아래 -> 왼쪽 아래 = 시계 방향 (D3D11은 시계 방향을 앞면으로 보고 그린다)
    // ---------------------------------------------------------------
    const Vertex vertices[] =
    {
        { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },   // 위: 빨강
        { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },   // 오른쪽 아래: 초록
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },   // 왼쪽 아래: 파랑
    };

    // ---------------------------------------------------------------
    // 2) 정점 버퍼: 위 배열을 GPU 메모리로 복사한다.
    // ---------------------------------------------------------------
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = sizeof(vertices);              // 전체 크기 = 28바이트 x 3개
    bufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;         // 만든 뒤 절대 바꾸지 않음 (GPU가 가장 다루기 쉬움)
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;      // 용도: 정점 버퍼

    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = vertices;                       // 처음 채워 넣을 CPU 쪽 데이터

    HRESULT hr = g_device->CreateBuffer(&bufferDesc, &initialData, &g_vertexBuffer);
    if (FAILED(hr))
    {
        PrintHResultError("정점 버퍼 생성", hr);
        return false;
    }

    // ---------------------------------------------------------------
    // 3) 셰이더: HLSL 파일을 컴파일하고, 디바이스로 셰이더 객체를 만든다.
    // ---------------------------------------------------------------
    const std::wstring shaderPath = GetShaderPath(L"BasicColor.hlsl");

    // 두 셰이더가 같은 파일에 있으므로, 문법 오류가 있으면 첫 번째에서 이미 실패한다.
    ComPtr<ID3DBlob> vsCode = CompileShader(shaderPath, "VSMain", "vs_5_0");
    if (!vsCode)
    {
        return false;
    }
    ComPtr<ID3DBlob> psCode = CompileShader(shaderPath, "PSMain", "ps_5_0");
    if (!psCode)
    {
        return false;
    }

    hr = g_device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &g_vertexShader);
    if (FAILED(hr))
    {
        PrintHResultError("정점 셰이더 생성", hr);
        return false;
    }

    hr = g_device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &g_pixelShader);
    if (FAILED(hr))
    {
        PrintHResultError("픽셀 셰이더 생성", hr);
        return false;
    }

    // ---------------------------------------------------------------
    // 4) 입력 레이아웃: "정점 하나의 바이트를 어떻게 읽는가" 설명서
    //    각 줄: { 셰이더의 시맨틱 이름, 번호, 데이터 형식, 버퍼 슬롯, 시작 위치(바이트), ... }
    // ---------------------------------------------------------------
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },  // float 3개, 0바이트부터
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },  // float 4개, 12바이트부터
    };

    // 정점 셰이더 코드를 함께 넘기는 이유:
    // 설명서의 이름(POSITION, COLOR)이 셰이더 입력(VSInput)과 맞는지 D3D가 검사하기 때문이다.
    hr = g_device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                     vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
                                     &g_inputLayout);
    if (FAILED(hr))
    {
        PrintHResultError("입력 레이아웃 생성", hr);
        return false;
    }

    std::printf("삼각형 준비 완료 (정점 3개, 셰이더: %ls)\n", shaderPath.c_str());
    return true;
}

int main()
{
    // 콘솔이 UTF-8 문자열을 올바르게 표시하도록 설정 (한글 깨짐 방지)
    SetConsoleOutputCP(CP_UTF8);

    std::printf("MiniRenderer: 프로그램 시작\n");
    OutputDebugStringA("MiniRenderer: 디버그 출력 테스트\n");

    // 이 프로그램(.exe) 자신을 가리키는 핸들. 창 등록/생성 때 "누구의 창인지" 알려준다.
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    // ---------------------------------------------------------------
    // 1) 윈도우 클래스 등록: 창의 "설계도"를 Windows에 알려준다.
    // ---------------------------------------------------------------
    const wchar_t* className = L"MiniRendererWindowClass";

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;                          // 쪽지를 받을 함수
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);   // 기본 화살표 커서
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);       // 기본 창 배경색
    wc.lpszClassName = className;                        // 설계도 이름

    if (!RegisterClassExW(&wc))
    {
        std::printf("[오류] 윈도우 클래스 등록 실패 (코드 %lu)\n", GetLastError());
        return 1;
    }

    // ---------------------------------------------------------------
    // 2) 창 생성
    //    우리가 원하는 건 "그림 그릴 영역(클라이언트 영역)"이 1280x720인 것.
    //    제목 표시줄과 테두리 두께만큼 전체 창 크기를 키워서 계산한다.
    // ---------------------------------------------------------------
    const int clientWidth  = 1280;
    const int clientHeight = 720;

    RECT rect = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,                                  // 확장 스타일 (없음)   
        className,                          // 위에서 등록한 설계도 이름
        L"MiniRenderer - 3단계",            // 창 제목
        WS_OVERLAPPEDWINDOW,                // 제목 표시줄, 테두리, 최소화/최대화/닫기 버튼이 있는 일반 창
        CW_USEDEFAULT, CW_USEDEFAULT,       // 화면 위치는 Windows가 알아서
        rect.right - rect.left,             // 테두리를 포함한 전체 너비
        rect.bottom - rect.top,             // 테두리를 포함한 전체 높이
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
    {
        std::printf("[오류] 창 생성 실패 (코드 %lu)\n", GetLastError());
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);   // 만들어진 창은 처음에 숨겨져 있으므로 보이게 한다.
    std::printf("창 생성 완료 (클라이언트 영역 %dx%d)\n", clientWidth, clientHeight);

    if (!InitD3D(hwnd, clientWidth, clientHeight))
    {
        return 1;
    }

    if (!InitTriangle())
    {
        PrintD3DDebugMessages();
        return 1;
    }
    PrintD3DDebugMessages();

    // ---------------------------------------------------------------
    // 3) 메시지 루프
    //    PeekMessage는 쪽지가 없으면 기다리지 않고 바로 돌아온다.
    //    그래서 쪽지를 다 처리한 뒤 "다른 일(나중에: 화면 그리기)"을 할 수 있다.
    // ---------------------------------------------------------------
    MSG msg = {};
    bool running = true;

    while (running)
    {
        // 쌓여 있는 쪽지를 모두 꺼내서 처리한다.
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)   // PostQuitMessage가 보낸 "끝내자" 쪽지
            {
                running = false;
                break;
            }
            TranslateMessage(&msg);   // 키 입력을 문자 입력 쪽지로 변환
            DispatchMessageW(&msg);   // 쪽지를 해당 창의 WndProc으로 배달
        }

        // ---- 한 프레임 그리기 ----
        // 색은 0.0~1.0 범위의 { 빨강, 초록, 파랑, 투명도 }. 지금은 짙은 파란색.
        const float clearColor[4] = { 0.05f, 0.10f, 0.30f, 1.0f };
        g_context->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);  // 백 버퍼를 한 색으로 칠함

        // 파이프라인(조립 라인)의 각 단계에 재료와 설정을 꽂는다.
        // FLIP 방식 스왑 체인은 Present 후 렌더 타깃 연결이 풀리므로 매 프레임 다시 설정한다.

        // [OM] 결과를 기록할 도화지: 백 버퍼
        g_context->OMSetRenderTargets(1, g_renderTargetView.GetAddressOf(), nullptr);

        // [RS] 뷰포트: -1~+1 좌표를 실제 픽셀 영역(0~1280, 0~720)으로 펼친다.
        D3D11_VIEWPORT viewport = {};
        viewport.Width    = static_cast<float>(clientWidth);
        viewport.Height   = static_cast<float>(clientHeight);
        viewport.MaxDepth = 1.0f;
        g_context->RSSetViewports(1, &viewport);

        // [IA] 재료 투입: 정점 버퍼 + 읽는 법(입력 레이아웃) + 정점을 묶는 방식(3개씩 삼각형)
        const UINT stride = sizeof(Vertex);   // 정점 하나의 크기: 다음 정점까지 몇 바이트 건너뛸지
        const UINT offset = 0;
        g_context->IASetInputLayout(g_inputLayout.Get());
        g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);
        g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // [VS] [PS] 사용할 셰이더
        g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
        g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);

        // 드로우 콜: 정점 3개를 0번부터 사용해 그려라!
        g_context->Draw(3, 0);

        // 백 버퍼를 화면에 내보낸다. 1 = 모니터 갱신 박자(VSync)에 맞춰 기다림.
        g_swapChain->Present(1, 0);

        PrintD3DDebugMessages();   // 이번 프레임에 디버그 레이어가 남긴 경고/오류 출력
    }

    std::printf("[메시지] WM_QUIT    : 메시지 루프 종료\n");

    // D3D 객체 해제: 만든 순서의 반대로 놓아준다.
    // (ComPtr는 프로그램이 끝날 때 자동으로 해제되지만, 언제 해제되는지 눈에 보이도록 직접 적는다.)
    g_inputLayout.Reset();
    g_pixelShader.Reset();
    g_vertexShader.Reset();
    g_vertexBuffer.Reset();
    g_renderTargetView.Reset();
    g_swapChain.Reset();
    g_context.Reset();
    g_infoQueue.Reset();
    g_device.Reset();
    std::printf("D3D11 객체 해제 완료\n");
    std::printf("MiniRenderer: 정상 종료\n");
    return static_cast<int>(msg.wParam);   // PostQuitMessage(0)에 넣은 0이 여기로 온다.
}
