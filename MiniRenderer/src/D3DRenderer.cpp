#include "D3DRenderer.h"
#include "Mesh.h"

#include <cstddef>        // offsetof: "구조체 안에서 이 칸은 몇 번째 바이트부터 시작해?"를 알려 준다
#include <cstdio>         // printf: 까만 콘솔 창에 글자 찍기
#include <string>         // std::wstring: 글자 여러 개를 담는 줄(문자열)
#include <vector>         // std::vector: 크기를 나중에 정할 수 있는 상자
#include <d3dcompiler.h>  // D3DCompileFromFile: 색칠 규칙 파일(.hlsl)을 그래픽 카드 말로 번역

using Microsoft::WRL::ComPtr;   // 매번 긴 이름을 쓰기 귀찮으니 ComPtr라고만 쓰겠다는 약속

// ===================================================================
// 이 파일 안에서만 쓰는 꼬마 도우미 함수들
//    namespace { } 로 감싸면 다른 파일에서는 안 보인다. (우리 집 안에서만 쓰는 물건)
// ===================================================================
namespace
{
    // 어떤 단계가 실패했는지, 실패 번호(HRESULT)와 함께 찍는다.
    // HRESULT = Windows가 알려 주는 "성공/실패 성적표" 숫자
    void PrintHResultError(const char* step, HRESULT hr)
    {
        std::printf("[오류] %s 실패 (HRESULT 0x%08lX)\n", step, static_cast<unsigned long>(hr));
    }

    // 색칠 규칙 파일이 어디 있는지 전체 주소를 만든다.
    //
    //    MiniRenderer
    //     ├─ build      <- 여기에 MiniRenderer.exe 가 있다
    //     └─ shaders    <- 여기에 BasicColor.hlsl 이 있다
    //
    //    exe가 있는 폴더에서 "한 칸 위로(..) 올라가서 shaders 폴더로" 가면 된다.
    //    exe 위치를 기준으로 찾으니까, 어디서 프로그램을 실행해도 파일을 잘 찾는다.
    std::wstring GetShaderPath(const wchar_t* fileName)
    {
        wchar_t exePath[MAX_PATH] = {};                   // 주소를 적을 빈 종이
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);   // "우리 exe 어디 있어?" -> 전체 주소를 받아 적는다

        std::wstring path = exePath;
        // 마지막 \ 뒤(파일 이름)를 잘라 내고 폴더 주소만 남긴다. 예: "C:\...\build\"
        path = path.substr(0, path.find_last_of(L"\\/") + 1);
        return path + L"..\\shaders\\" + fileName;        // 한 칸 위로 -> shaders 폴더 -> 파일 이름
    }

    // 색칠 규칙 파일(.hlsl) 안의 함수 하나를 그래픽 카드가 알아듣는 말로 번역한다.
    //    entryPoint: 번역할 함수 이름 (예: "VSMain")
    //    target    : 어떤 종류로 번역할지
    //                "vs_5_0" = 꼭짓점 담당(정점 셰이더), "ps_5_0" = 픽셀 담당(픽셀 셰이더)
    ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entryPoint, const char* target)
    {
        ComPtr<ID3DBlob> code;     // 번역 결과 (그래픽 카드용 코드)
        ComPtr<ID3DBlob> errors;   // 번역 실패 시 "어디가 틀렸어요" 쪽지

        HRESULT hr = D3DCompileFromFile(
            path.c_str(), nullptr, nullptr,
            entryPoint, target,
            D3DCOMPILE_DEBUG,      // 나중에 문제 찾기 쉽게 번역해 줘
            0, &code, &errors);

        if (FAILED(hr))   // 번역 실패!
        {
            if (errors)
            {
                // 받아쓰기 틀린 것처럼 문법이 틀렸다. 파일 이름, 몇 번째 줄, 뭐가 틀렸는지 찍는다.
                std::printf("[셰이더 오류] %s\n", static_cast<const char*>(errors->GetBufferPointer()));
            }
            else
            {
                // 틀린 곳 쪽지조차 없으면, 보통 파일을 못 찾은 것이다.
                std::printf("[오류] 셰이더 파일을 열 수 없음: %ls\n", path.c_str());
                PrintHResultError("셰이더 컴파일", hr);
            }
            return nullptr;   // 빈손으로 돌아간다
        }
        return code;   // 번역 성공! 결과를 돌려준다
    }
}

// ===================================================================
// 소멸자: 화가가 퇴근할 때 빌린 물건을 그래픽 카드에 반납한다.
//    블록 쌓기를 위에서부터 치우듯, 만든 순서의 "반대"로 반납한다.
//    (ComPtr가 알아서 반납해 주긴 하지만, 언제 반납되는지 눈으로 보려고 직접 적었다)
// ===================================================================
D3DRenderer::~D3DRenderer()
{
    m_inputLayout.Reset();        // Reset() = "이거 다 썼어요, 반납!"
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_renderTargetView.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_infoQueue.Reset();
    m_device.Reset();             // 만들기 창구는 맨 처음 만들었으니 맨 마지막에 반납
    std::printf("D3D11 객체 해제 완료\n");
}

// ===================================================================
// Initialize: 그림 그릴 준비 3단계
//    1) 그래픽 카드와 연결 + 그림판 받기
//    2) 그림판에 "여기에 그려" 이름표 붙이기
//    3) 색칠 규칙(셰이더) 준비
// ===================================================================
bool D3DRenderer::Initialize(HWND hwnd, int width, int height)
{
    m_width  = width;
    m_height = height;

    // && 는 "그리고". 앞 단계가 실패하면 뒤 단계는 아예 하지 않고 바로 false가 된다.
    // (도미노처럼 하나가 넘어지면 거기서 멈춤)
    const bool ok = CreateDeviceAndSwapChain(hwnd)
                 && CreateRenderTarget()
                 && CreateBasicColorShader();

    PrintDebugMessages();   // 준비하는 동안 그래픽 카드가 남긴 잔소리가 있으면 읽어 준다
    return ok;
}

// ===================================================================
// CreateDeviceAndSwapChain: 그래픽 카드와 연결하고, 그림판 2장을 받는다.
// ===================================================================
bool D3DRenderer::CreateDeviceAndSwapChain(HWND hwnd)
{
    // ---------------------------------------------------------------
    // 1) 그림판 주문서 작성
    //
    //    왜 그림판이 2장일까?
    //      그림판 1장에 그리면, 그리는 도중의 어수선한 모습이 화면에 보여서 깜빡거린다.
    //      그래서 2장을 쓴다.
    //        - 앞 그림판(프런트 버퍼): 지금 화면에 보이는 그림
    //        - 뒤 그림판(백 버퍼)   : 안 보이는 곳에서 몰래 그리는 그림
    //      다 그리면 두 장을 휙 맞바꾼다(Swap). 그래서 이름이 스왑 체인!
    // ---------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC scd = {};   // 빈 주문서
    scd.BufferDesc.Width  = m_width;                         // 그림판 가로 = 창 안쪽 가로
    scd.BufferDesc.Height = m_height;                        // 그림판 세로 = 창 안쪽 세로
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;      // 점 하나 = 빨강, 초록, 파랑, 투명도 (각각 0~255)
    scd.SampleDesc.Count  = 1;                               // 울퉁불퉁한 가장자리 다듬기(안티앨리어싱): 안 함
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 용도: 여기에 그림을 그릴 거예요
    scd.BufferCount       = 2;                               // 그림판 2장
    scd.OutputWindow      = hwnd;                            // 어느 창에 보여 줄지 = App이 만든 창의 번호표
    scd.Windowed          = TRUE;                            // 전체 화면 말고 창 모드
    scd.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_DISCARD;   // 맞바꾸는 방식 (요즘 Windows가 추천하는 방법)

    // ---------------------------------------------------------------
    // 2) 그래픽 카드에 연결!  한 번에 3가지를 받아 온다.
    //      - 디바이스(만들기 창구)
    //      - 디바이스 컨텍스트(일 시키기 창구)
    //      - 스왑 체인(그림판 2장 세트)
    //
    //    먼저 "디버그 레이어"를 켜고 시도한다.
    //    디버그 레이어 = 우리가 실수하면 잔소리해 주는 선생님 모드.
    //    컴퓨터에 선생님이 설치 안 되어 있으면, 끄고 다시 시도한다.
    // ---------------------------------------------------------------
    UINT flags = D3D11_CREATE_DEVICE_DEBUG;   // 선생님 모드 켜기
    D3D_FEATURE_LEVEL featureLevel = {};      // 그래픽 카드가 할 수 있는 기능 수준을 받아 적을 칸

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // 어느 그래픽 카드? -> 기본 그래픽 카드
        D3D_DRIVER_TYPE_HARDWARE,   // 진짜 그래픽 카드를 쓴다 (흉내 내는 소프트웨어 말고)
        nullptr,                    // 소프트웨어 그래픽 도구: 안 씀
        flags,                      // 선생님 모드 켜기/끄기
        nullptr, 0,                 // 기능 수준: 그래픽 카드가 할 수 있는 제일 높은 걸로 알아서
        D3D11_SDK_VERSION,          // "저는 D3D11 버전으로 만들었어요"
        &scd,                       // 아까 쓴 그림판 주문서
        &m_swapChain,               // 받은 그림판 세트를 여기에 넣어 줘
        &m_device,                  // 받은 만들기 창구를 여기에
        &featureLevel,              // 기능 수준을 여기에 적어 줘
        &m_context);                // 받은 일 시키기 창구를 여기에

    // "선생님이 설치 안 되어 있어요" 실패라면 -> 선생님 모드 끄고 한 번 더!
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        std::printf("[안내] 디버그 레이어가 없어 끄고 진행합니다 (그래픽 도구 미설치)\n");
        flags = 0;   // 선생님 모드 끄기
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION,
            &scd, &m_swapChain, &m_device, &featureLevel, &m_context);
    }
    if (FAILED(hr))   // 그래도 실패하면 포기
    {
        PrintHResultError("디바이스/스왑 체인 생성", hr);
        return false;
    }
    std::printf("D3D11 디바이스 생성 완료 (기능 수준 0x%X, 디버그 레이어 %s)\n",
                static_cast<unsigned>(featureLevel), flags ? "켜짐" : "꺼짐");

    // 선생님 모드가 켜졌다면, 선생님의 잔소리 우편함도 받아 둔다.
    // (As = "이 창구, 잔소리 우편함 모양으로도 쓸 수 있지? 그 모양으로 줘")
    if (flags & D3D11_CREATE_DEVICE_DEBUG)
    {
        m_device.As(&m_infoQueue);
    }
    return true;
}

// ===================================================================
// CreateRenderTarget: 뒤 그림판에 "여기에 그려" 이름표를 붙인다.
//    그래픽 카드는 그림판 그 자체가 아니라 이 이름표(렌더 타깃 뷰)를 보고 그린다.
// ===================================================================
bool D3DRenderer::CreateRenderTarget()
{
    // 그림판 세트에서 0번 그림판(뒤 그림판)을 잠깐 꺼내 온다.
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr))
    {
        PrintHResultError("백 버퍼 가져오기", hr);
        return false;
    }

    // 만들기 창구에게 "이 그림판에 붙일 이름표 만들어 줘" 하고 부탁한다.
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
    if (FAILED(hr))
    {
        PrintHResultError("렌더 타깃 뷰 생성", hr);
        return false;
    }
    // 이 함수가 끝나면 backBuffer 손잡이는 저절로 놓아진다.
    // 그래도 그림판은 사라지지 않는다. 그림판 세트와 이름표가 계속 꽉 잡고 있기 때문이다.
    return true;
}

// ===================================================================
// CreateBasicColorShader: 색칠 규칙(셰이더)을 준비한다.
//
//    셰이더 = 그래픽 카드 안에서 돌아가는 아주 작은 프로그램
//      - 정점 셰이더: 꼭짓점마다 한 번씩 실행. "이 꼭짓점은 화면 어디?"
//      - 픽셀 셰이더: 화면의 점(픽셀)마다 한 번씩 실행. "이 점은 무슨 색?"
//    셰이더는 C++이 아니라 HLSL이라는 그래픽 카드 전용 언어로 쓴다. (shaders\BasicColor.hlsl)
// ===================================================================
bool D3DRenderer::CreateBasicColorShader()
{
    // ---------------------------------------------------------------
    // 1) 셰이더 파일을 번역하고, 그래픽 카드에 셰이더를 만든다.
    // ---------------------------------------------------------------
    const std::wstring shaderPath = GetShaderPath(L"BasicColor.hlsl");

    // 두 셰이더가 같은 파일에 들어 있다.
    // 그래서 파일에 틀린 글자가 있으면 첫 번째 번역에서 바로 걸린다.
    ComPtr<ID3DBlob> vsCode = CompileShader(shaderPath, "VSMain", "vs_5_0");   // 꼭짓점 담당 번역
    if (!vsCode)
    {
        return false;
    }
    ComPtr<ID3DBlob> psCode = CompileShader(shaderPath, "PSMain", "ps_5_0");   // 픽셀 담당 번역
    if (!psCode)
    {
        return false;
    }

    // 번역된 코드(내용물 위치, 크기)를 만들기 창구에 주고 "정점 셰이더 만들어 줘"
    HRESULT hr = m_device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr))
    {
        PrintHResultError("정점 셰이더 생성", hr);
        return false;
    }

    // 마찬가지로 "픽셀 셰이더 만들어 줘"
    hr = m_device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr))
    {
        PrintHResultError("픽셀 셰이더 생성", hr);
        return false;
    }

    // ---------------------------------------------------------------
    // 2) 입력 레이아웃: 꼭짓점 데이터 읽는 법 설명서
    //
    //    그래픽 카드한테 꼭짓점 데이터는 그냥 숫자가 줄줄이 이어진 덩어리다.
    //    어디까지가 위치고 어디부터가 색인지 알려 줘야 한다. (Mesh.h의 Vertex 모양 기준)
    //
    //    꼭짓점 하나 = 28바이트
    //    [ x  y  z ][ 빨 초 파 투명 ]
    //     0바이트~   12바이트~
    //     (위치)      (색)
    //
    //    각 줄에 적는 것:
    //      { 셰이더에서 부르는 이름, 같은 이름 중 몇 번째, 숫자 모양, 몇 번 통로, 몇 바이트부터 시작, ... }
    // ---------------------------------------------------------------
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },  // 위치: 소수 3개, 0바이트부터
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },  // 색: 소수 4개, 12바이트부터
    };

    // 설명서를 만들 때 정점 셰이더 코드도 같이 보여 준다.
    // 왜? 설명서의 이름(POSITION, COLOR)과 셰이더가 기다리는 이름이 똑같은지
    // 그래픽 카드 쪽에서 미리 맞춰 보기 때문이다. (짝 맞추기 검사)
    hr = m_device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                     vsCode->GetBufferPointer(), vsCode->GetBufferSize(),
                                     &m_inputLayout);
    if (FAILED(hr))
    {
        PrintHResultError("입력 레이아웃 생성", hr);
        return false;
    }

    std::printf("셰이더 준비 완료 (%ls)\n", shaderPath.c_str());
    return true;
}

// ===================================================================
// 그림 그리기는 공장의 조립 라인(파이프라인)처럼 여러 단계를 거친다.
//
//   [IA] 재료 넣기   : 꼭짓점 데이터를 읽어 온다
//   [VS] 정점 셰이더 : 꼭짓점 위치를 정한다
//   [RS] 래스터라이저: 삼각형 안에 들어가는 점(픽셀)들을 찾는다
//   [PS] 픽셀 셰이더 : 점마다 색을 정한다
//   [OM] 출력        : 그림판에 색을 찍는다
//
// 아래 함수들은 각 단계에 "이 재료, 이 도구 써" 하고 꽂아 주는 일을 한다.
// 함수 이름 앞의 IA, VS, RS, PS, OM 이 바로 그 단계 이름이다.
// ===================================================================

// ===================================================================
// BeginFrame: 그림 한 장 시작! 칠판 지우기 + 어디에 그릴지 정하기
// ===================================================================
void D3DRenderer::BeginFrame(const float clearColor[4])
{
    // 뒤 그림판 전체를 clearColor 한 가지 색으로 싹 칠한다. (칠판 지우기)
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    // 그림판을 맞바꾸고 나면(Present) 연결이 풀려 버린다.
    // 그래서 매번 그림 그리기 전에 다시 꽂아 줘야 한다.

    // [OM] "결과는 이 그림판에 찍어 줘"
    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);

    // [RS] 뷰포트: 모눈종이 좌표(-1 ~ +1)를 진짜 화면 픽셀 크기로 쭉 늘려 준다.
    //    예) 가로: -1 -> 0번째 픽셀,  +1 -> 1280번째 픽셀
    //        세로: +1 -> 맨 위,       -1 -> 맨 아래
    D3D11_VIEWPORT viewport = {};
    viewport.Width    = static_cast<float>(m_width);    // 그림판 가로 전체를 쓴다
    viewport.Height   = static_cast<float>(m_height);   // 그림판 세로 전체를 쓴다
    viewport.MaxDepth = 1.0f;                           // 깊이(앞뒤) 범위: 0 ~ 1
    m_context->RSSetViewports(1, &viewport);
}

// ===================================================================
// Draw: 도형 하나 그리기
// ===================================================================
void D3DRenderer::Draw(const Mesh& mesh)
{
    // [IA] "꼭짓점 데이터는 이 설명서대로 읽어"
    m_context->IASetInputLayout(m_inputLayout.Get());

    // [VS] [PS] "이 색칠 규칙으로 그려"
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // 꼭짓점 데이터 꽂기와 진짜 "그려!" 명령은 도형(Mesh)이 직접 한다.
    // 자기 데이터는 자기가 제일 잘 아니까.
    mesh.Draw(m_context.Get());
}

// ===================================================================
// EndFrame: 그림 한 장 끝! 화면에 짠!
// ===================================================================
void D3DRenderer::EndFrame()
{
    // 뒤 그림판과 앞 그림판을 휙 맞바꿔서, 방금 그린 그림을 화면에 보여 준다.
    // 1 = 모니터가 화면을 새로 그리는 박자에 맞춰 기다리기 (VSync)
    //     이렇게 하면 화면이 찢어져 보이지 않고, 쓸데없이 너무 빨리 그리지도 않는다.
    m_swapChain->Present(1, 0);

    PrintDebugMessages();   // 이번 그림을 그리는 동안 선생님이 남긴 잔소리가 있으면 읽어 준다
}

// ===================================================================
// PrintDebugMessages: 선생님(디버그 레이어)의 잔소리 우편함을 열어서 읽어 준다.
//    원래 이 잔소리는 Visual Studio 같은 도구에서만 보인다.
//    까만 콘솔 창에서도 보이게 하려고 직접 꺼내서 찍는다.
// ===================================================================
void D3DRenderer::PrintDebugMessages()
{
    if (!m_infoQueue)
    {
        return;   // 선생님 모드가 꺼져 있으면 우편함도 없다. 그냥 돌아간다.
    }

    const UINT64 count = m_infoQueue->GetNumStoredMessages();   // 쪽지가 몇 장 쌓였나?
    for (UINT64 i = 0; i < count; ++i)                          // 한 장씩 차례대로
    {
        // 쪽지마다 길이가 다르다.
        // 그래서 먼저 "이 쪽지 얼마나 길어?" 물어보고, 그만한 상자를 준비한 다음 내용을 받는다.
        SIZE_T size = 0;
        m_infoQueue->GetMessage(i, nullptr, &size);    // 1) 크기만 물어보기
        std::vector<char> buffer(size);                // 2) 그 크기만큼 빈 상자 준비
        D3D11_MESSAGE* message = reinterpret_cast<D3D11_MESSAGE*>(buffer.data());   // 상자를 쪽지 모양으로 보기
        m_infoQueue->GetMessage(i, message, &size);    // 3) 상자에 내용 받기

        // 쪽지가 얼마나 심각한지 본다.
        // "참고로 알려 줄게" 정도의 쪽지는 건너뛰고, 문제가 있는 것만 찍는다.
        const char* level = nullptr;
        switch (message->Severity)
        {
        case D3D11_MESSAGE_SEVERITY_CORRUPTION: level = "CORRUPTION"; break;   // 아주 큰일 (메모리가 망가짐)
        case D3D11_MESSAGE_SEVERITY_ERROR:      level = "ERROR";      break;   // 틀렸어!
        case D3D11_MESSAGE_SEVERITY_WARNING:    level = "WARNING";    break;   // 조심해!
        default: break;                                                         // 나머지는 무시
        }
        if (level)
        {
            std::printf("[D3D11 %s] %s\n", level, message->pDescription);
        }
    }
    m_infoQueue->ClearStoredMessages();   // 다 읽은 쪽지는 우편함에서 비운다
}
