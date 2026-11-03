#pragma once   // 이 파일을 여러 번 불러와도 한 번만 읽게 하는 약속

// ===================================================================
// App: 프로그램 전체를 이끄는 "반장"
//
//    반장이 하는 일:
//      1) 창(도화지)을 만든다.
//      2) 프로그램이 끝날 때까지 빙글빙글 돌면서
//         - 우편함의 쪽지(마우스, 키보드, X 버튼 등)를 처리하고
//         - 화가(D3DRenderer)에게 "그림 한 장 그려!" 하고 시킨다.
//
//    직접 그림을 그리지는 않는다. 그건 화가의 일이다.
// ===================================================================

#include <windows.h>        // 창 만들기, 쪽지 받기 등 Windows 기본 도구 상자

#include "D3DRenderer.h"    // 화가
#include "Mesh.h"           // 그릴 도형 상자

class App
{
public:
    App() = default;   // 만들 때: 특별히 할 일 없음

    // 창과 그래픽 카드 물건은 하나씩만 있어야 한다.
    // 반장을 복사해서 두 명이 되면 같은 물건을 서로 반납하려다 꼬이니까 복사 금지!
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // 준비 운동: 창 만들기 -> 화가 준비 -> 삼각형 준비
    // clientWidth, clientHeight = 그림 그릴 칸의 가로, 세로 크기
    bool Initialize(int clientWidth, int clientHeight);

    // 프로그램의 심장. 끝날 때까지 쪽지 처리 + 그림 그리기를 반복한다.
    // 끝나면 "잘 끝났어요(0)" 같은 종료 번호를 돌려준다.
    int Run();

private:
    // ---------------------------------------------------------------
    // 쪽지 받는 함수가 왜 2개나 있을까?
    //
    //    Windows는 우리 App이 누군지 모른다.
    //    그래서 App 안의 평범한 함수는 직접 부를 수가 없다.
    //
    //    WindowProc   : 안내 데스크. static(정적)이라서 App 없이도 부를 수 있다.
    //                   Windows는 이 함수에 쪽지를 준다.
    //                   안내 데스크는 창에 메모해 둔 App 주소를 보고 쪽지를 넘겨준다.
    //    HandleMessage: 진짜 쪽지 처리 담당. App 안의 함수라서
    //                   App의 물건(m_hwnd 등)을 마음대로 쓸 수 있다.
    //
    //    CALLBACK = "Windows가 나중에 불러 줄 함수예요"라는 표시
    // ---------------------------------------------------------------
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow();   // 창(도화지) 만들기
    void RenderFrame();        // 그림 한 장 그리기 (지우기 -> 그리기 -> 짠!)

    HWND m_hwnd         = nullptr;   // 창의 번호표 (nullptr = 아직 창 없음)
    int  m_clientWidth  = 0;         // 그림 그릴 칸의 가로 크기
    int  m_clientHeight = 0;         // 그림 그릴 칸의 세로 크기

    // ---------------------------------------------------------------
    // 적는 순서가 중요하다!
    //
    //    물건은 적힌 순서대로 만들어지고, "거꾸로" 치워진다. (블록 쌓기처럼)
    //      만들기: m_renderer -> m_triangle
    //      치우기: m_triangle -> m_renderer
    //
    //    삼각형 상자는 화가의 만들기 창구로 만든 것이다.
    //    그러니 삼각형을 먼저 반납하고, 화가가 나중에 퇴근해야 깔끔하다.
    //    (만든 사람보다 만든 물건을 먼저 치우는 게 자연스러운 순서)
    // ---------------------------------------------------------------
    D3DRenderer m_renderer;   // 화가
    Mesh        m_triangle;   // 그릴 삼각형
};
