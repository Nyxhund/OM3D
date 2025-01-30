#ifndef IMGUIRENDERER_H
#define IMGUIRENDERER_H

#include <Material.h>
#include <chrono>
#include <imgui/IconsFontAwesome5.h>

struct ImDrawData;
struct GLFWwindow;

namespace OM3D
{

    class ImGuiRenderer : NonMovable
    {
    public:
        ImGuiRenderer(GLFWwindow* window);

        void start();
        void finish();
        u32 _debug_texture = 0;
        int octaves_noise = 8;
        float worley_cell_nb = 7.0f;
        float worley_cell_additional = 3.0f;
        bool generate_texture_noise = true;
        bool generate_texture_weather = true;

    private:
        void render(const ImDrawData* draw_data);
        float update_delta_time();

        GLFWwindow* _window = nullptr;

        Material _material;
        std::unique_ptr<Texture> _font;
        std::chrono::time_point<std::chrono::high_resolution_clock> _last;
    };

} // namespace OM3D

#endif // IMGUIRENDERER_H
