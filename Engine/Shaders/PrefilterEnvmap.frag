#version 330 core

uniform samplerCube EnvMap;
uniform int Face;
uniform sampler2D EnvTex;
uniform bool Skybox;
uniform float Roughness;

in vec3 pass_position;
in vec2 pass_texCoords;

out vec4 fragColor;

const float PI = 3.1415926535897932384626433832795;
const float PI_OVER_TWO = PI / 2.0;
const float TWO_PI = PI * 2.0;

const float ONE_OVER_PI = 1.0 / PI;
const float ONE_OVER_TWO_PI = 1.0 / TWO_PI;

float RadicalInverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint NumSamples) {
    return vec2(float(i) / float(NumSamples), RadicalInverse(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, float Roughness, vec3 N) {
    float a = Roughness * Roughness;
    
    float Phi = 2 * PI * Xi.x;
    
    float CosTheta = sqrt( (1 - Xi.y) / ( 1 + (a*a - 1) * Xi.y ) );
    float SinTheta = sqrt(1 - CosTheta * CosTheta);
    
    vec3 H = vec3(SinTheta * cos(Phi), SinTheta * sin(Phi), CosTheta);

    // Transform H to normal basis
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 TangentX = normalize(cross(N, UpVector));
    vec3 TangentY = cross(N, TangentX);
    
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

vec3 toLinear(vec3 gammaColor) {
    return pow(gammaColor, vec3(2.2));
}

vec2 toUV(vec3 dir) {
    float phi = atan(dir.z, dir.x) - PI_OVER_TWO;
    float theta = asin(-dir.y) + PI_OVER_TWO;
    return vec2(phi * ONE_OVER_TWO_PI, theta * ONE_OVER_PI);
}

vec3 PrefilterEnvMap(float Roughness, vec3 R)
{
    vec3 N = R;
    vec3 V = R;
    
    vec3 Color = vec3(0, 0, 0);
    
    uint NumSamples = 512u;
    float TotalWeight = 0.0;
    for (uint i = 0u; i < NumSamples; i++)
    {
        vec2 Xi = Hammersley(i, NumSamples);
        vec3 H = ImportanceSampleGGX(Xi, Roughness, N);
        vec3 L = normalize(reflect(-V, H));
        
        float NdotL = max(0, dot(N, L));
        
        if (NdotL > 0)
        {
            if (Skybox) {
                Color += texture(EnvMap, L).rgb * NdotL;
            }
            else {
                vec2 uv = toUV(L);

                vec3 skySample = min(textureLod(EnvTex, uv, 0).rgb, 1000);
                Color += skySample * NdotL;
            }
            TotalWeight += NdotL;
        }
    }
    
    return Color / TotalWeight;
}

void main()
{
    mat3 rots[6] = mat3[]
    (
        mat3(vec3(0, 0, -1), vec3(0, -1, 0), vec3(-1, 0, 0)), // Right
        mat3(vec3(0, 0, 1), vec3(0, -1, 0), vec3(1, 0, 0)), // Left
        mat3(vec3(1, 0, 0), vec3(0, 0, 1), vec3(0, -1, 0)), // Top
        mat3(vec3(1, 0, 0), vec3(0, 0, -1), vec3(0, 1, 0)), // Bottom
        mat3(vec3(1, 0, 0), vec3(0, -1, 0), vec3(0, 0, -1)), // Front
        mat3(vec3(-1, 0, 0), vec3(0, -1, 0), vec3(0, 0, 1))  // Back
    );

    vec3 dir = normalize(vec3(pass_texCoords * 2 - 1, -1));
    dir = rots[Face] * dir;

    vec3 color;
    if (Skybox) {
        color = toLinear(PrefilterEnvMap(Roughness, dir));
    }
    else {
        color = PrefilterEnvMap(Roughness, dir);
    }
    
    fragColor = vec4(color, 1);
}
