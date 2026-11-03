#include "Mesh.h"

#include <cstdio>   // printf: 까만 콘솔 창에 글자 찍기

// ===================================================================
// Create: 꼭짓점 목록을 그래픽 카드 메모리로 택배 보내기
//
//    컴퓨터 본체(CPU)와 그래픽 카드(GPU)는 쓰는 메모리가 따로따로다.
//    그래픽 카드는 자기 메모리에 있는 것만 빨리 볼 수 있으니까,
//    그리기 전에 꼭짓점 데이터를 그래픽 카드 쪽으로 옮겨 놔야 한다.
//
//    순서: 상자 주문서 쓰기 -> 상자에 넣을 내용물 정하기 -> 만들기 창구에 부탁하기
// ===================================================================
bool Mesh::Create(ID3D11Device* device, const Vertex* vertices, UINT vertexCount)
{
    // 1) 그래픽 카드 쪽 상자(정점 버퍼) 주문서
    D3D11_BUFFER_DESC bufferDesc = {};   // 빈 주문서
    bufferDesc.ByteWidth = sizeof(Vertex) * vertexCount;  // 상자 크기 = 꼭짓점 하나 28바이트 x 꼭짓점 개수
    bufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;         // 한 번 넣으면 절대 안 바꿈 (그래픽 카드가 제일 편하게 다룬다)
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;      // 용도: 꼭짓점을 담는 상자

    // 2) 상자를 만들자마자 채워 넣을 내용물
    //    안 바꾸는 상자(IMMUTABLE)라서, 만들 때 꼭 내용물을 같이 줘야 한다.
    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = vertices;                       // CPU 쪽에 있는 꼭짓점 목록

    // 3) 만들기 창구에게 "이 주문서대로, 이 내용물을 넣어서 상자 만들어 줘"
    //    다 만든 상자는 m_vertexBuffer에 담아 준다.
    HRESULT hr = device->CreateBuffer(&bufferDesc, &initialData, &m_vertexBuffer);
    if (FAILED(hr))   // 실패!
    {
        std::printf("[오류] 정점 버퍼 생성 실패 (HRESULT 0x%08lX)\n", static_cast<unsigned long>(hr));
        return false;
    }

    m_vertexCount = vertexCount;   // 꼭짓점이 몇 개인지 기억해 둔다 (그릴 때 필요)
    return true;                   // 택배 도착 완료!
}

// ===================================================================
// Draw: 내 꼭짓점들을 그래픽 카드에 꽂고 "그려!" 하기
//    조립 라인의 맨 앞 [IA](재료 넣기) 단계를 맡는다.
// ===================================================================
void Mesh::Draw(ID3D11DeviceContext* context) const
{
    // [IA] 재료 넣기 1: 꼭짓점 상자 꽂기
    //    stride: 꼭짓점 하나의 크기(28바이트).
    //            그래픽 카드는 28바이트씩 건너뛰면서 다음 꼭짓점을 읽는다.
    //    offset: 상자 맨 앞(0바이트)부터 읽기 시작해
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    // (0번 통로부터, 상자 1개를, 이 상자로, 이 크기씩, 여기서부터)
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    // [IA] 재료 넣기 2: 꼭짓점을 어떻게 묶을지
    //    TRIANGLELIST = 꼭짓점을 3개씩 묶어서 삼각형 하나로 만든다.
    //    (꼭짓점 6개면 삼각형 2개, 9개면 삼각형 3개)
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 드로우 콜: "꼭짓점 m_vertexCount개를, 0번째부터 써서 그려!"
    // 이 한 줄이 진짜로 그래픽 카드를 움직이게 하는 명령이다.
    context->Draw(m_vertexCount, 0);
}
