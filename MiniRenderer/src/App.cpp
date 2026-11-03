#include "App.h"

#include <cstdio>   // printf(글자를 까만 콘솔 창에 찍는 함수)를 쓰려고 가져온다.

// ===================================================================
// Initialize: 프로그램이 시작할 때 딱 한 번 하는 "준비 운동"
//    1) 창(도화지)을 만든다.
//    2) 그림 그리는 도구(렌더러)를 준비한다.
//    3) 그릴 삼각형을 준비한다.
//    하나라도 실패하면 false(실패!)를 돌려주고 멈춘다.
// ===================================================================
bool App::Initialize(int clientWidth, int clientHeight)
{
    // 그림 그릴 공간의 가로, 세로 크기를 기억해 둔다.
    m_clientWidth  = clientWidth;
    m_clientHeight = clientHeight;

    // 1) 창 만들기. 실패하면 여기서 포기!
    if (!CreateMainWindow())
    {
        return false;
    }

    // 2) 그림 도구 준비하기. "이 창(m_hwnd)에 그릴 거야"라고 알려 준다.
    if (!m_renderer.Initialize(m_hwnd, m_clientWidth, m_clientHeight))
    {
        return false;
    }

    // ---------------------------------------------------------------
    // 3) 그릴 삼각형의 꼭짓점 3개 (꼭짓점 = 정점, 영어로 Vertex)
    //
    //    화면을 모눈종이라고 생각하자.
    //      - 정가운데가 (0, 0)
    //      - 왼쪽 끝은 -1, 오른쪽 끝은 +1
    //      - 아래쪽 끝은 -1, 위쪽 끝은 +1
    //
    //               (0, 0.5) 빨강
    //                  ▲
    //                 ▲ ▲
    //                ▲▲▲▲
    //   (-0.5,-0.5) 파랑   (0.5,-0.5) 초록
    //
    //    꼭짓점 하나마다 { 위치(x, y, z) }, { 색(빨, 초, 파, 투명도) } 를 적는다.
    //    색은 0.0(하나도 없음) ~ 1.0(가득) 사이 숫자로 섞는다.
    //
    //    순서는 위 -> 오른쪽 아래 -> 왼쪽 아래, 즉 시계 방향!
    //    그래픽 카드는 시계 방향으로 놓인 삼각형을 "앞면"으로 보고 그린다.
    //    (반시계로 적으면 "뒷면이네?" 하고 안 그려 버린다.)
    //
    //    지금 이 숫자들은 아직 컴퓨터 본체(CPU) 쪽 메모리에 있다.
    //    그래픽 카드(GPU)는 아직 이 삼각형을 모른다.
    // ---------------------------------------------------------------
    const Vertex vertices[] =
    {
        //   위치(x,    y,    z)       색(빨,   초,   파,   투명도)
        { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },   // 위: 빨강
        { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },   // 오른쪽 아래: 초록
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },   // 왼쪽 아래: 파랑
    };
    // (숫자 뒤의 f는 "소수점 있는 숫자(float)야"라는 표시)

    // 꼭짓점 3개를 그래픽 카드에게 택배로 보낸다.
    // ARRAYSIZE(vertices)는 "배열에 몇 개 들어 있니?" -> 3
    if (!m_triangle.Create(m_renderer.GetDevice(), vertices, ARRAYSIZE(vertices)))
    {
        return false;
    }
    std::printf("삼각형 준비 완료 (정점 %u개)\n", m_triangle.GetVertexCount());
    return true;   // 준비 운동 끝! 성공!
}

// ===================================================================
// CreateMainWindow: 화면에 창(도화지)을 하나 만든다.
//    순서: 설계도 만들기 -> Windows에 등록 -> 설계도대로 창 만들기 -> 보이게 하기
// ===================================================================
bool App::CreateMainWindow()
{
    // 우리 프로그램(.exe)의 "이름표".
    // Windows에게 "이 창은 이 프로그램 거예요"라고 알려 줄 때 쓴다.
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    // ---------------------------------------------------------------
    // 1) 창의 "설계도" 만들기
    //    붕어빵 틀을 먼저 만드는 것과 같다.
    //    틀(설계도)이 있어야 붕어빵(창)을 찍어 낼 수 있다.
    // ---------------------------------------------------------------
    const wchar_t* className = L"MiniRendererWindowClass";   // 설계도에 붙일 이름

    WNDCLASSEXW wc = {};   // 빈 신청서 한 장. {} 는 "모든 칸을 0으로 비워 둬"
    wc.cbSize        = sizeof(wc);                       // 신청서 크기 (Windows가 확인용으로 물어본다)
    wc.lpfnWndProc   = WindowProc;                       // 창에 무슨 일이 생기면 쪽지를 받을 함수
    wc.hInstance     = hInstance;                        // 이 설계도의 주인 = 우리 프로그램
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);   // 창 위의 마우스 모양 = 평범한 화살표
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);       // 창 배경색 = Windows 기본색
    wc.lpszClassName = className;                        // 설계도 이름

    // 다 쓴 설계도를 Windows에 제출한다. (&wc 는 "wc가 있는 곳의 주소")
    if (!RegisterClassExW(&wc))
    {
        // 실패하면 왜 실패했는지 번호(오류 코드)를 찍고 포기.
        std::printf("[오류] 윈도우 클래스 등록 실패 (코드 %lu)\n", GetLastError());
        return false;
    }

    // ---------------------------------------------------------------
    // 2) 창 크기 계산하기
    //    창은 "제목 줄 + 테두리 + 안쪽 그림 칸"으로 되어 있다.
    //
    //      ┌───────────────────┐ <- 제목 줄
    //      │                   │
    //      │   그림 그리는 칸   │ <- 우리가 원하는 크기는 이 부분!
    //      │                   │
    //      └───────────────────┘ <- 테두리
    //
    //    안쪽 칸을 딱 원하는 크기로 만들려면
    //    제목 줄과 테두리 두께만큼 창 전체를 더 크게 만들어야 한다.
    //    AdjustWindowRect가 그 계산을 대신 해 준다.
    // ---------------------------------------------------------------
    RECT rect = { 0, 0, m_clientWidth, m_clientHeight };   // 네모: 왼, 위, 오른, 아래
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);   // FALSE = 메뉴 줄은 없음

    // ---------------------------------------------------------------
    // 3) 설계도대로 진짜 창 만들기
    //    재료가 많아서 한 줄에 하나씩 적었다.
    //    성공하면 창의 "번호표(HWND)"를 돌려준다. 앞으로 창을 부를 때 이 번호표를 쓴다.
    // ---------------------------------------------------------------
    m_hwnd = CreateWindowExW(
        0,                                  // 특별한 옵션: 없음
        className,                          // 아까 등록한 설계도 이름
        L"MiniRenderer - 3단계",            // 창 맨 위 제목 줄에 보일 글자
        WS_OVERLAPPEDWINDOW,                // 평범한 창 (제목 줄, 테두리, _ □ X 버튼 있음)
        CW_USEDEFAULT, CW_USEDEFAULT,       // 화면 어디에 띄울지: Windows가 알아서 정해 줘
        rect.right - rect.left,             // 창 전체 너비 (테두리 포함) = 오른쪽 - 왼쪽
        rect.bottom - rect.top,             // 창 전체 높이 (제목 줄 포함) = 아래 - 위
        nullptr,                            // 부모 창: 없음 (혼자 있는 창)
        nullptr,                            // 메뉴: 없음
        hInstance,                          // 이 창의 주인 = 우리 프로그램
        this);                              // "나(App) 여기 있어요" 주소 쪽지를 같이 보낸다
                                            // (아래 WindowProc에서 이 쪽지를 꺼내 쓴다)

    // 번호표를 못 받았으면 창 만들기 실패!
    if (!m_hwnd)
    {
        std::printf("[오류] 창 생성 실패 (코드 %lu)\n", GetLastError());
        return false;
    }

    // 막 만든 창은 투명 망토를 쓴 것처럼 숨어 있다. 이제 보이게 한다!
    ShowWindow(m_hwnd, SW_SHOW);
    std::printf("창 생성 완료 (클라이언트 영역 %dx%d)\n", m_clientWidth, m_clientHeight);
    return true;
}

// ===================================================================
// Run: 프로그램이 끝날 때까지 계속 빙글빙글 도는 "심장"
//
//    한 바퀴마다 하는 일:
//      1) 우편함에 쪽지가 있나 확인 (마우스 클릭, 키보드, X 버튼 등)
//      2) 쪽지가 있으면 전부 처리
//      3) 그림을 한 장 그린다
//    이걸 1초에 수십~수백 번 반복하면 화면이 계속 그려진다.
//    (만화책을 빠르게 넘기면 움직이는 것처럼 보이는 것과 같은 원리)
// ===================================================================
int App::Run()
{
    MSG msg = {};          // 우편함에서 꺼낸 쪽지를 담을 그릇
    bool running = true;   // "아직 계속 돌아?" 스위치

    while (running)   // 스위치가 켜져 있는 동안 계속 반복
    {
        // 우편함에 쌓인 쪽지를 하나씩 꺼낸다.
        // PeekMessage는 "쪽지 있어?" 하고 슬쩍 보기만 한다.
        //   - 있으면: 꺼내 온다 (PM_REMOVE = 꺼낸 쪽지는 우편함에서 빼기)
        //   - 없으면: 기다리지 않고 바로 다음으로 넘어간다
        // 기다리지 않으니까 쪽지가 없을 때도 그림을 계속 그릴 수 있다.
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // "이제 프로그램 끝내자!" 쪽지가 왔으면
            if (msg.message == WM_QUIT)
            {
                running = false;   // 스위치 끄기
                break;             // 쪽지 꺼내기 그만!
            }
            TranslateMessage(&msg);   // "A 키 눌림" 쪽지를 "글자 a 입력" 쪽지로 바꿔 준다
            DispatchMessageW(&msg);   // 쪽지를 우체부처럼 WindowProc에게 배달한다
        }

        // 아직 끝나지 않았으면 그림 한 장 그리기
        if (running)
        {
            RenderFrame();
        }
    }

    std::printf("[메시지] WM_QUIT    : 메시지 루프 종료\n");
    // PostQuitMessage(0) 에 넣었던 0을 꺼내서 "잘 끝났어요(0)"라고 돌려준다.
    return static_cast<int>(msg.wParam);
}

// ===================================================================
// RenderFrame: 그림 한 장(한 프레임) 그리기
//    칠판 그림 그리기와 똑같다.
//      1) 칠판을 깨끗이 지운다 (배경색으로 싹 칠하기)
//      2) 삼각형을 그린다
//      3) 다 그린 그림을 화면에 "짠!" 하고 보여 준다
// ===================================================================
void App::RenderFrame()
{
    // 배경색 { 빨강, 초록, 파랑, 투명도 } (0.0 ~ 1.0)
    // 파랑이 제일 많으니까 -> 짙은 남색
    const float clearColor[4] = { 0.05f, 0.10f, 0.30f, 1.0f };

    m_renderer.BeginFrame(clearColor);   // 1) 남색으로 싹 지우고, 어디에 그릴지 정한다
    m_renderer.Draw(m_triangle);         // 2) 삼각형 그리기 (색칠 규칙(셰이더)도 여기서 정한다)
    m_renderer.EndFrame();               // 3) 화면에 짠! (Present) + 문제 있으면 알려 주기
}

// ===================================================================
// WindowProc: 창으로 오는 쪽지를 맨 처음 받는 "안내 데스크"
//
//    문제: Windows는 우리 App 객체가 누군지 모른다.
//          그래서 App 안의 함수(HandleMessage)를 직접 부를 수가 없다.
//
//    해결: 이 안내 데스크가 중간 다리가 된다.
//      1) 창이 막 태어날 때(WM_NCCREATE) 함께 온 "App 주소 쪽지"를 꺼내서
//         창 번호표 뒷면에 메모해 둔다. (GWLP_USERDATA = 메모 칸)
//      2) 그 뒤로 오는 모든 쪽지는 메모를 보고 App을 찾아가
//         App의 HandleMessage에게 넘겨준다.
// ===================================================================
LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 창이 막 태어나는 순간에 오는 쪽지라면
    if (msg == WM_NCCREATE)
    {
        // 쪽지 봉투(lParam)를 "창 만들기 정보" 모양으로 열어 본다.
        const CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        // 그 안에 든 App 주소(CreateWindowExW 때 보낸 this)를 창 번호표 뒷면에 메모!
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    }

    // 창 번호표 뒷면에 적힌 메모를 읽어서 App 주소로 바꾼다.
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app)
    {
        // 메모가 있다! App에게 쪽지를 넘겨준다.
        return app->HandleMessage(hwnd, msg, wParam, lParam);
    }

    // WM_NCCREATE보다 먼저 도착하는 쪽지도 몇 개 있다.
    // 그때는 아직 메모가 없으니까 Windows에게 "알아서 해 주세요" 하고 맡긴다.
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ===================================================================
// HandleMessage: 쪽지를 읽고 진짜로 처리하는 곳
//    switch는 "쪽지 종류가 뭐야?" 하고 골라서 맞는 곳으로 가는 갈림길이다.
// ===================================================================
LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        // 사용자가 창 오른쪽 위 X 버튼을 눌렀다!
        // -> 창을 부순다. (부수면 곧 WM_DESTROY 쪽지가 온다)
        std::printf("[메시지] WM_CLOSE   : X 버튼 클릭 -> 창을 파괴합니다\n");
        DestroyWindow(hwnd);
        return 0;   // "이 쪽지는 내가 처리했어요"

    case WM_DESTROY:
        // 창이 완전히 사라졌다.
        // -> 번호표를 버리고, 우편함에 "이제 끝내자(WM_QUIT)" 쪽지를 넣는다.
        //    그러면 Run()의 반복이 그 쪽지를 보고 멈춘다.
        std::printf("[메시지] WM_DESTROY : 창이 사라짐 -> 종료를 요청합니다\n");
        m_hwnd = nullptr;
        PostQuitMessage(0);   // 0 = "문제 없이 잘 끝났어요"
        return 0;
    }

    // 우리가 신경 쓰지 않는 나머지 쪽지(창 옮기기, 크기 바꾸기 등)는
    // Windows가 원래 하던 대로 처리하게 맡긴다.
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
