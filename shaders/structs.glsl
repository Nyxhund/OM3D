struct CameraData {
    mat4 view_proj;
    vec3 camera_pos;
};

struct FrameData {
    CameraData camera;

    vec3 sun_dir;
    uint point_light_count;

    vec3 sun_color;
    float padding_1;
};

struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float padding_1;
};

struct CloudData {
    CameraData camera;
    // vec2 resolution;
};
