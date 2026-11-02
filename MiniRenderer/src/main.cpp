// 2단계: D3D11 초기화와 화면 지우기
// 목표: 매 프레임 백 버퍼를 한 색으로 지우고 화면에 표시한다.

#include <cstdio>
#include <windows.h>      // 창, 메시지, OutputDebugStringA 등 Windows 기본 기능
#include <d3d11.h>        // Direct3D 11
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

// 실패한 단계 이름과 HRESULT 값을 출력한다.
void PrintHResultError(const char* step, HRESULT hr)
{
    std::printf("[오류] %s 실패 (HRESULT 0x%08lX)\n", step, static_cast<unsigned long>(hr));
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
        L"MiniRenderer - 2단계",            // 창 제목
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

        // 백 버퍼를 화면에 내보낸다. 1 = 모니터 갱신 박자(VSync)에 맞춰 기다림.
        g_swapChain->Present(1, 0);
    }

    std::printf("[메시지] WM_QUIT    : 메시지 루프 종료\n");

    // D3D 객체 해제: 만든 순서의 반대로 놓아준다.
    // (ComPtr는 프로그램이 끝날 때 자동으로 해제되지만, 언제 해제되는지 눈에 보이도록 직접 적는다.)
    g_renderTargetView.Reset();
    g_swapChain.Reset();
    g_context.Reset();
    g_device.Reset();
    std::printf("D3D11 객체 해제 완료\n");
    std::printf("MiniRenderer: 정상 종료\n");
    return static_cast<int>(msg.wParam);   // PostQuitMessage(0)에 넣은 0이 여기로 온다.
}
