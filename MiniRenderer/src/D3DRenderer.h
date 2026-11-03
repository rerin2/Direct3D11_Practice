#pragma once   // 이 파일을 여러 번 불러와도 한 번만 읽게 하는 약속

// ===================================================================
// D3DRenderer: 그래픽 카드(GPU)와 이야기하는 일을 몽땅 맡는 "화가"
//
//    App이 "그려!" 하고 시키면, 이 화가가 그래픽 카드에게 말을 걸어 그림을 그린다.
//    매 그림(프레임)마다 하는 일:  칠판 지우기 -> 그리기 -> 화면에 짠! 보여 주기
//
//    창에 오는 쪽지(마우스, X 버튼 등)는 신경 쓰지 않는다. 그건 App의 일이다.
//    (일을 나눠 맡으면 코드가 덜 헷갈린다)
// ===================================================================

#include <d3d11.h>        // Direct3D 11: 그래픽 카드에게 말을 거는 Windows 도구 상자
#include <wrl/client.h>   // ComPtr: 다 쓴 물건을 알아서 반납해 주는 똑똑한 손잡이

class Mesh;   // "Mesh라는 친구가 어딘가 있어" 하고 이름만 미리 알려 둔다 (자세한 건 Mesh.h에)

class D3DRenderer
{
public:
    D3DRenderer() = default;   // 만들 때: 특별히 할 일 없음
    ~D3DRenderer();            // 없어질 때: 빌린 물건들을 그래픽 카드에 반납한다

    // 화가는 딱 한 명만 있어야 한다. 복사해서 두 명이 되면 안 되니까 복사 금지!
    D3DRenderer(const D3DRenderer&) = delete;
    D3DRenderer& operator=(const D3DRenderer&) = delete;

    // 그림 그릴 준비. "hwnd 창에, 가로 width x 세로 height 크기로 그릴게요"
    bool Initialize(HWND hwnd, int width, int height);

    // 그래픽 카드 쪽 "만들기 창구"를 빌려준다.
    // Mesh 같은 다른 친구가 그래픽 카드에 물건(삼각형 데이터 등)을 만들 때 쓴다.
    ID3D11Device* GetDevice() const { return m_device.Get(); }

    // ---- 그림 한 장(한 프레임) 그리는 3단계 ----
    void BeginFrame(const float clearColor[4]);   // 1) 칠판을 이 색으로 지우고, 어디에 그릴지 정한다
    void Draw(const Mesh& mesh);                  // 2) 도형 하나 그리기
    void EndFrame();                              // 3) 다 그린 그림을 화면에 짠! + 문제 있으면 알려 주기

private:
    // Initialize 안에서 차례대로 부르는 준비 단계들
    bool CreateDeviceAndSwapChain(HWND hwnd);   // 그래픽 카드와 연결하고, 그림판 2장 받기
    bool CreateRenderTarget();                  // "이 그림판에 그려" 이름표 붙이기
    bool CreateBasicColorShader();              // 색칠 규칙(셰이더) 준비하기
    void PrintDebugMessages();                  // 그래픽 카드가 남긴 잔소리(경고) 읽어 주기

    int m_width  = 0;   // 그림판 가로 크기 (픽셀)
    int m_height = 0;   // 그림판 세로 크기 (픽셀)

    // ---------------------------------------------------------------
    // 그래픽 카드에서 빌려 온 물건들
    //    ComPtr<...> 는 "똑똑한 손잡이"다.
    //    손잡이를 놓으면(화가가 없어지면) 빌린 물건을 자동으로 반납한다.
    // ---------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11Device>           m_device;            // 만들기 창구: 그래픽 카드에 물건을 만든다
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_context;           // 일 시키기 창구: "지워!", "그려!" 명령을 보낸다
    Microsoft::WRL::ComPtr<IDXGISwapChain>         m_swapChain;         // 그림판 2장 세트 (하나는 보여 주고, 하나는 몰래 그린다)
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;  // 그림판에 붙인 "여기에 그려" 이름표
    Microsoft::WRL::ComPtr<ID3D11InfoQueue>        m_infoQueue;         // 그래픽 카드의 잔소리(경고) 우편함 (디버그 모드일 때만)

    Microsoft::WRL::ComPtr<ID3D11VertexShader>     m_vertexShader;      // 꼭짓점마다 "화면 어디에 둘까?"를 정하는 작은 프로그램
    Microsoft::WRL::ComPtr<ID3D11PixelShader>      m_pixelShader;       // 점(픽셀)마다 "무슨 색으로 칠할까?"를 정하는 작은 프로그램
    Microsoft::WRL::ComPtr<ID3D11InputLayout>      m_inputLayout;       // 꼭짓점 데이터 읽는 법 설명서 (앞쪽은 위치, 뒤쪽은 색)
};
