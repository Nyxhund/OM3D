#include "utils.glsl"
#include "worley3D.glsl"
#include "perlin.glsl"

#define M_PI 3.1415926535897932384626433832795

uniform float threshold = 0.0f;

float remap(float originalValue, float originalMin, float originalMax,
    float newMin, float newMax)
{
    return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float fbm_perlin_3D(vec3 p, uint octaves) {
    float res = 0.0;
    float amp = 0.5;
    float freq = 0.92;

    for (int i = 0; i < octaves; i++) {
        res += amp * noise(p * freq);
        freq *= 2.0;
        amp *= 0.5;
    }

    return res;
}

float eval_density_normalized(const vec3 p, uint octaves)
{
    float res = (1 + fbm_perlin_3D(p, octaves)) * 0.5;
    return res;
}

vec3 position_to_normalized(vec3 position, vec3 pointMin, vec3 pointMax)
{
    return vec3(
        remap(position.x, pointMin.x, pointMax.x, 0.0, 1.0),
        remap(position.y, pointMin.y, pointMax.y, 0.0, 1.0),
        remap(position.z, pointMin.z, pointMax.z, 0.0, 1.0)
    );
}
