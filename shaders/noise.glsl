#include "utils.glsl"
#include "worley3D.glsl"
#include "perlin.glsl"

#define M_PI 3.1415926535897932384626433832795

uniform uint octaves = 3;
uniform float worley_cell_nb = 1.0f;
uniform float threshold = 0.0f;

float remap(float originalValue, float originalMin, float originalMax,
            float newMin, float newMax)
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

float eval_density_normalized(const vec3 p)
{
    float freq = 1;
    float res = (1 + fbm(p * freq)) * 0.5;
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

float compute_noise(vec3 position, const vec3 pointMin, const vec3 pointMax) {

    // position = position_to_normalized(position, pointMin, pointMax);

    // float final = 1.0 - worley(position, worley_cell_nb);

    // float final = eval_density_normalized(position * 8);

    const float worleyNoise0 = (1.0f - worley(position, worley_cell_nb * 2.0));
    const float worleyNoise1 = (1.0f - worley(position, worley_cell_nb * 8.0));
    const float worleyNoise2 = (1.0f - worley(position, worley_cell_nb * 14.0));

    float worleyFBM = worleyNoise0 * 0.625 + worleyNoise1 * 0.25 + worleyNoise2 * 0.125;

    float perlin = eval_density_normalized(position * 8);
    float final = remap(perlin, 0.0f, 1.0f, worleyFBM, 1.0f);
    return max(final - threshold, 0.0f);
}
