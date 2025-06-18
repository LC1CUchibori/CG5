#include "Test.hlsli"

struct PixelShaderOutput
{
    
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    
    output.color = float32_t4(uv.x, uv.y, 0.0f, 1.0f);
    
    return output;
};