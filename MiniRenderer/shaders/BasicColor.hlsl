// 3단계: 정점의 위치와 색을 그대로 화면에 그리는 가장 단순한 셰이더

// 정점 셰이더가 받는 입력: C++의 Vertex 구조체와 짝이 맞아야 한다.
// ": POSITION", ": COLOR" 는 "시맨틱(이름표)"이다. 입력 레이아웃의 이름과 이것으로 연결된다.
struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

// 정점 셰이더 -> (래스터화) -> 픽셀 셰이더로 전달되는 데이터
struct PSInput
{
    float4 position : SV_POSITION;   // SV_ = 시스템이 쓰는 값. 화면 위치 (-1 ~ +1 좌표)
    float4 color    : COLOR;         // 래스터화 단계에서 세 꼭짓점 사이로 섞여(보간되어) 들어온다
};

// 정점 셰이더: 정점마다 한 번 실행된다. (삼각형이면 3번)
PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0f);   // 위치를 그대로 화면 좌표로 사용
    output.color    = input.color;                    // 색은 그대로 전달
    return output;
}

// 픽셀 셰이더: 삼각형이 덮는 픽셀마다 한 번 실행된다. (수만 번)
// SV_TARGET = 이 반환값을 렌더 타깃(백 버퍼)에 기록하라는 뜻
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
