#include "utils.glsl"
#include "worley3D.glsl"
#include "perlin.glsl"

#define M_PI 3.1415926535897932384626433832795

uniform float jitter = 0;
uniform uint octaves = 1;

float remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
    return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float fbm(vec3 p) {
    float res = 0.0;
    float amp = 1.0;
    float freq = 1.0;

    for (int i = 0; i < octaves; i++) { // Adjust number of octaves
        res += amp * noise(p * freq);
        freq *= 2.0;
        amp *= 0.5;
    }

    return res;
}

float eval_density_abs(const vec3 p) {
    float base_density = fbm(p);
    return abs(base_density); // Adjust the noise evaluation as needed
}

float eval_density_normalized(const vec3 p)
{
    float freq = 1;
    float res = (1 + fbm(p * freq)) * 0.5;
    return res;
}

float compute_noise(const vec3 position) {
    const float frequenceMul[5] = float[](8.0, 14.0, 20.0, 26.0, 32.0);

    // float final = 1.0 - worley(position * 8, jitter, false).x;

    // float final = noise(position);

    const float worleyNoise0 = (1.0f - worley(position * frequenceMul[0], jitter, false).x);
    const float worleyNoise1 = (1.0f - worley(position * frequenceMul[1], jitter, false).x);
    const float worleyNoise2 = (1.0f - worley(position * frequenceMul[2], jitter, false).x);

    float worleyFBM = worleyNoise0 * 0.625 + worleyNoise1 * 0.25 + worleyNoise2 * 0.125;

    float perlin = eval_density_normalized(position * 8);
    float final = remap(perlin, 0.0f, 1.0f, worleyFBM, 1.0f);
    return final;
}
