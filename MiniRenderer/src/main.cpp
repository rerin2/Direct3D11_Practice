// 진입점: App을 만들고 실행한다.
// 창은 App, GPU는 D3DRenderer, 도형 데이터는 Mesh가 맡는다.

#include <cstdio>
#include <windows.h>

#include "App.h"

int main()
{
    // 콘솔이 UTF-8 문자열을 올바르게 표시하도록 설정 (한글 깨짐 방지)
    SetConsoleOutputCP(CP_UTF8);

    std::printf("MiniRenderer: 프로그램 시작\n");
    OutputDebugStringA("MiniRenderer: 디버그 출력 테스트\n");

    int exitCode = 1;
    {
        App app;
        if (app.Initialize(1280, 720))
        {
            exitCode = app.Run();
        }
    }   // 여기서 app이 파괴되며 삼각형 -> 렌더러 순으로 GPU 객체가 해제된다.

    if (exitCode == 0)
    {
        std::printf("MiniRenderer: 정상 종료\n");
    }
    return exitCode;
}
