#pragma once   // 이 파일을 여러 번 불러와도 한 번만 읽게 하는 약속

// ===================================================================
// Mesh: "그릴 도형 하나"를 담아 두는 상자
//
//    도형의 꼭짓점(정점) 데이터를 그래픽 카드(GPU)의 메모리로 옮겨 놓고,
//    "그려!" 하면 그 데이터를 그래픽 카드에 꽂아 준다.
//
//    어떤 색칠 규칙(셰이더)으로 그릴지는 모른다. 그건 화가(D3DRenderer)의 일이다.
//    Mesh는 "나는 이런 모양이야"만 알고 있다.
// ===================================================================

#include <d3d11.h>        // Direct3D 11: 그래픽 카드에게 말을 거는 Windows 도구 상자
#include <wrl/client.h>   // ComPtr: 다 쓴 물건을 알아서 반납해 주는 똑똑한 손잡이

// ---------------------------------------------------------------
// Vertex: 꼭짓점 하나에 적는 정보
//
//    [ x  y  z ][ 빨 초 파 투명 ]
//     위치 12바이트   색 16바이트   = 모두 28바이트
//
//    (소수 하나(float) = 4바이트. 위치는 3개라서 12, 색은 4개라서 16)
//
//    셰이더(BasicColor.hlsl)의 VSInput도 이것과 순서와 크기가 똑같아야 한다.
//    짝이 안 맞으면 위치를 색으로 읽는 것처럼 엉뚱한 그림이 나온다.
// ---------------------------------------------------------------
struct Vertex
{
    float position[3];   // 위치: x(가로), y(세로), z(앞뒤)           -> 12바이트
    float color[4];      // 색: 빨강, 초록, 파랑, 투명도 (0.0 ~ 1.0)  -> 16바이트
};

class Mesh
{
public:
    // 컴퓨터 본체(CPU) 쪽에 있는 꼭짓점 목록을 그래픽 카드(GPU) 메모리로 복사한다.
    // 택배 보내기와 같다.
    //    device      : 그래픽 카드의 "만들기 창구" (D3DRenderer에게 빌려 온다)
    //    vertices    : 보낼 꼭짓점 목록의 첫 번째 위치
    //    vertexCount : 꼭짓점이 몇 개인지 (삼각형이면 3)
    // 성공하면 true, 실패하면 false
    bool Create(ID3D11Device* device, const Vertex* vertices, UINT vertexCount);

    // 그래픽 카드에 꼭짓점 데이터를 꽂고 "그려!" 명령을 보낸다.
    // 단, 화가(D3DRenderer)가 색칠 규칙과 읽는 법 설명서를 먼저 꽂아 둔 상태여야 한다.
    // 끝의 const = "이 함수는 Mesh 안의 내용을 바꾸지 않아요"라는 약속
    void Draw(ID3D11DeviceContext* context) const;

    // "꼭짓점 몇 개야?" 물어보면 대답해 준다.
    UINT GetVertexCount() const { return m_vertexCount; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;   // 그래픽 카드 쪽에 있는 꼭짓점 상자 (정점 버퍼)
    UINT m_vertexCount = 0;                                // 꼭짓점 개수 (UINT = 0 이상인 정수)
};
