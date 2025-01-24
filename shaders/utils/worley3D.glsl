vec3 random3(vec3 p) {
    return fract(sin(vec3(dot(p,vec3(127.1,311.7, 167.5)),dot(p,vec3(269.5,183.3, 758.2)), dot(p,vec3(269.5,183.3, 758.2))))*43758.5453);
}

// Inspired from Book of shaders article about Cellular Noise (Worley)
float worley(vec3 position, float cellScale) {
    // Scale
    position *= cellScale;

    // Tile the space
    vec3 i_st = floor(position);
    vec3 f_st = fract(position);

    float m_dist = 1.;  // minimum distance

    for (int z = -1; z <= 1; z++) {
        for (int y= -1; y <= 1; y++) {
            for (int x= -1; x <= 1; x++) {
                // Neighbor place in the grid
                vec3 neighbor = vec3(float(x),float(y), float(z));

                // Random position from current + neighbor place in the grid
                vec3 point = random3(i_st + neighbor);

                // Vector between the pixel and the point
                vec3 diff = neighbor + point - f_st;

                // Distance to the point
                float dist = length(diff);

                // Keep the closer distance
                m_dist = min(m_dist, dist);
            }
        }
    }

    return m_dist;
}

#pragma glslify: export(worley)
