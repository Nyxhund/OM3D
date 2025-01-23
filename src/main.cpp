

#include <array>
#include <glad/gl.h>
#include <iterator>

#include "ImageFormat.h"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "imgui/imgui_internal.h"
#include "utils.h"

#define GLFW_INCLUDE_NONE
#include <Framebuffer.h>
#include <GLFW/glfw3.h>
#include <ImGuiRenderer.h>
#include <Scene.h>
#include <Texture.h>
#include <Texture3D.h>
#include <TimestampQuery.h>
#include <filesystem>
#include <graphics.h>
#include <imgui/imgui.h>
#include <iostream>
#include <shader_structs.h>
#include <vector>

using namespace OM3D;

static float delta_time = 0.0f;
static std::unique_ptr<Scene> scene;
static float exposure = 1.0;
static std::vector<std::string> scene_files;

// Sun Light
static glm::vec3 light_pos = glm::vec3(0.0, 5.0, 0.0);
static bool sun_debug;
static float light_intensity = 10.f;

// Phase function parameters
static float g0 = -0.2f;
static float g1 = 0.6f;
static float w = 0.3f;

// Raymarching parameters
static float step_size = 1.0f;

// Light scattering coefficients
static float sigma_a = 0.005f;
static float sigma_s = 0.11f;
// According to `Real time Rendering 4th edition`, albedo ~= sigma_s && sigma_s
// + sigma_a c= [0.06, 0.12] in the ccase of cloud

namespace OM3D
{
    extern bool audit_bindings_before_draw;
}

void parse_args(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];

        if (arg == "--validate")
        {
            OM3D::audit_bindings_before_draw = true;
        }
        else
        {
            std::cerr << "Unknown argument \"" << arg << "\"" << std::endl;
        }
    }
}

// static bool in_plane(const glm::vec3& n, const glm::vec3& p,
//                      const glm::vec3 center, float radius)
// {
//     glm::vec3 v = center + glm::normalize(n) * radius;
//     glm::vec3 x = v - p;
//     return glm::dot(x, n) >= 0;
// }

void glfw_check(bool cond)
{
    if (!cond)
    {
        const char* err = nullptr;
        glfwGetError(&err);
        std::cerr << "GLFW error: " << err << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void update_delta_time()
{
    static double time = 0.0;
    const double new_time = program_time();
    delta_time = float(new_time - time);
    time = new_time;
}

void process_inputs(GLFWwindow* window, Camera& camera)
{
    static glm::dvec2 mouse_pos;

    glm::dvec2 new_mouse_pos;
    glfwGetCursorPos(window, &new_mouse_pos.x, &new_mouse_pos.y);

    {
        glm::vec3 movement = {};
        if (glfwGetKey(window, 'W') == GLFW_PRESS)
        {
            movement += camera.forward();
        }
        if (glfwGetKey(window, 'S') == GLFW_PRESS)
        {
            movement -= camera.forward();
        }
        if (glfwGetKey(window, 'D') == GLFW_PRESS)
        {
            movement += camera.right();
        }
        if (glfwGetKey(window, 'A') == GLFW_PRESS)
        {
            movement -= camera.right();
        }

        float speed = 10.0f;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        {
            speed *= 10.0f;
        }

        if (movement.length() > 0.0f)
        {
            const glm::vec3 new_pos =
                camera.position() + movement * delta_time * speed;
            camera.set_view(
                glm::lookAt(new_pos, new_pos + camera.forward(), camera.up()));
        }
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        const glm::vec2 delta = glm::vec2(mouse_pos - new_mouse_pos) * 0.01f;
        if (delta.length() > 0.0f)
        {
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), delta.x,
                                        glm::vec3(0.0f, 1.0f, 0.0f));
            rot = glm::rotate(rot, delta.y, camera.right());
            camera.set_view(glm::lookAt(
                camera.position(),
                camera.position() + (glm::mat3(rot) * camera.forward()),
                (glm::mat3(rot) * camera.up())));
        }
    }

    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(window, &width, &height);
        camera.set_ratio(float(width) / float(height));
    }

    mouse_pos = new_mouse_pos;
}

void gui(ImGuiRenderer& imgui)
{
    const ImVec4 error_text_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    const ImVec4 warning_text_color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
    (void)warning_text_color;

    static bool open_gpu_profiler = false;

    PROFILE_GPU("GUI");

    imgui.start();
    DEFER(imgui.finish());

    // ImGui::ShowDemoWindow();

    bool open_scene_popup = false;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Scene"))
            {
                open_scene_popup = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Exposure"))
        {
            ImGui::DragFloat("Exposure", &exposure, 0.25f, 0.01f, 100.0f,
                             "%.2f", ImGuiSliderFlags_Logarithmic);
            if (exposure != 1.0f && ImGui::Button("Reset"))
            {
                exposure = 1.0f;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug"))
        {
            ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
            if (ImGui::Selectable("None", imgui._debug_texture == 3))
                imgui._debug_texture = 3;
            if (ImGui::Selectable("Albedo", imgui._debug_texture == 0))
                imgui._debug_texture = 0;
            if (ImGui::Selectable("Normal", imgui._debug_texture == 1))
                imgui._debug_texture = 1;
            if (ImGui::Selectable("Depth", imgui._debug_texture == 2))
                imgui._debug_texture = 2;
            if (ImGui::Selectable("Wireframe Light", imgui._debug_texture == 4))
                imgui._debug_texture = 4;
            if (ImGui::Selectable("Texture Generator",
                                  imgui._debug_texture == 5))
                imgui._debug_texture = 5;
            ImGui::PopItemFlag();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Light"))
        {
            static float light_position[3] = { 0.0f, 5.0f,
                                               0.0f }; // Default position
            if (ImGui::DragFloat3("Light Position", light_position, 0.1f,
                                  -100.0f, 100.0f, "%.2f"))
            {
                light_pos = glm::vec3(light_position[0], light_position[1],
                                      light_position[2]);
            }
            if (ImGui::Button("Reset"))
            {
                light_position[0] = 0.0f;
                light_position[1] = 5.0f;
                light_position[2] = 0.0f;
                light_pos = glm::vec3(light_position[0], light_position[1],
                                      light_position[2]);
            }

            ImGui::DragFloat("Intensity", &light_intensity, 0.25f, 0.01f,
                             100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
            if (ImGui::Button("Reset"))
            {
                light_intensity = 10.0f;
            }
            ImGui::Checkbox("Visualize Light Pos", &sun_debug);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Clouds"))
        {
            ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_None;
            if (ImGui::TreeNodeEx("Phase parameters", flag))
            {
                ImGui::DragFloat("g0", &g0, 0.01f, -1.0f, 1.0f, "%.2f",
                                 ImGuiSliderFlags_Logarithmic);
                if (g0 != 0.6f && ImGui::Button("Reset"))
                {
                    g0 = 0.6f;
                }

                ImGui::DragFloat("g1", &g1, 0.01f, -1.0f, 1.0f, "%.2f",
                                 ImGuiSliderFlags_Logarithmic);
                if (g1 != -0.2f && ImGui::Button("Reset"))
                {
                    g1 = -0.2f;
                }

                ImGui::DragFloat("w", &w, 0.01f, 0.0f, 1.0f, "%.2f",
                                 ImGuiSliderFlags_Logarithmic);
                if (w != 0.3f && ImGui::Button("Reset"))
                {
                    w = 0.3f;
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Light scattering coeffiients", flag))
            {
                ImGui::DragFloat("absorption coeff (sigma a)", &sigma_a, 0.001f,
                                 0.0f, 1.0f, "%.3f",
                                 ImGuiSliderFlags_Logarithmic);
                if (sigma_a != 0.005f && ImGui::Button("Reset"))
                {
                    sigma_a = 0.005f;
                }

                ImGui::DragFloat("scattering coeff (sigma s)", &sigma_s, 0.001f,
                                 0.0f, 1.0f, "%.3f",
                                 ImGuiSliderFlags_Logarithmic);
                if (sigma_s != 0.11f && ImGui::Button("Reset"))
                {
                    sigma_s = 0.11f;
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Raymarching parameters", flag))
            {
                ImGui::DragFloat("step size", &step_size, 0.01f, 0.01f, 100.0f,
                                 "%.2f", ImGuiSliderFlags_Logarithmic);
                if (step_size != 1.0f && ImGui::Button("Reset"))
                {
                    step_size = 1.0f;
                }
                ImGui::TreePop();
            }
            ImGui::EndMenu();
        }

        if (scene && ImGui::BeginMenu("Scene Info"))
        {
            ImGui::Text("%u objects", u32(scene->objects().size()));
            ImGui::Text("%u point lights", u32(scene->point_lights().size()));
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("GPU Profiler"))
        {
            open_gpu_profiler = true;
        }

        ImGui::Separator();
        ImGui::TextUnformatted(
            reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

        ImGui::Separator();
        ImGui::Text("%.2f ms", delta_time * 1000.0f);

#ifdef OM3D_DEBUG
        ImGui::Separator();
        ImGui::TextColored(warning_text_color, ICON_FA_BUG " (DEBUG)");
#endif

        if (!bindless_enabled())
        {
            ImGui::Separator();
            ImGui::TextColored(error_text_color,
                               ICON_FA_EXCLAMATION_TRIANGLE
                               " Bindless textures not supported");
        }
        ImGui::EndMainMenuBar();
    }

    if (open_scene_popup)
    {
        ImGui::OpenPopup("###openscenepopup");

        scene_files.clear();
        for (auto&& entry : std::filesystem::directory_iterator(data_path))
        {
            if (entry.status().type() == std::filesystem::file_type::regular)
            {
                const auto ext = entry.path().extension();
                if (ext == ".gltf" || ext == ".glb")
                {
                    scene_files.emplace_back(entry.path().string());
                }
            }
        }
    }

    if (ImGui::BeginPopup("###openscenepopup",
                          ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto load_scene = [](const std::string path) {
            auto result = Scene::from_gltf(path);
            if (!result.is_ok)
            {
                std::cerr << "Unable to load scene (" << path << ")"
                          << std::endl;
            }
            else
            {
                scene = std::move(result.value);
            }
            ImGui::CloseCurrentPopup();
        };

        char buffer[1024] = {};
        if (ImGui::InputText("Load scene", buffer, sizeof(buffer),
                             ImGuiInputTextFlags_EnterReturnsTrue))
        {
            load_scene(buffer);
        }

        if (!scene_files.empty())
        {
            for (const std::string& p : scene_files)
            {
                const auto abs = std::filesystem::absolute(p).string();
                if (ImGui::MenuItem(abs.c_str()))
                {
                    load_scene(p);
                    break;
                }
            }
        }

        ImGui::EndPopup();
    }

    if (open_gpu_profiler)
    {
        if (ImGui::Begin(ICON_FA_CLOCK " GPU Profiler"))
        {
            const ImGuiTableFlags table_flags = ImGuiTableFlags_SortTristate
                | ImGuiTableFlags_NoSavedSettings
                | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV
                | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg;

            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,
                                  ImVec4(1, 1, 1, 0.01f));
            DEFER(ImGui::PopStyleColor());

            if (ImGui::BeginTable("##timetable", 3, table_flags))
            {
                ImGui::TableSetupColumn("Name",
                                        ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("CPU (ms)",
                                        ImGuiTableColumnFlags_NoResize, 70.0f);
                ImGui::TableSetupColumn("GPU (ms)",
                                        ImGuiTableColumnFlags_NoResize, 70.0f);
                ImGui::TableHeadersRow();

                std::vector<u32> indents;
                for (const auto& zone : retrieve_profile())
                {
                    auto color_from_time = [](float time) {
                        const float t =
                            std::min(time / 0.008f, 1.0f); // 8ms = red
                        return ImVec4(t, 1.0f - t, 0.0f, 1.0f);
                    };

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(zone.name.data());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          color_from_time(zone.cpu_time));
                    ImGui::Text("%.2f", zone.cpu_time * 1000.0f);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          color_from_time(zone.gpu_time));
                    ImGui::Text("%.2f", zone.gpu_time * 1000.0f);

                    ImGui::PopStyleColor(2);

                    if (!indents.empty() && --indents.back() == 0)
                    {
                        indents.pop_back();
                        ImGui::Unindent();
                    }

                    if (zone.contained_zones)
                    {
                        indents.push_back(zone.contained_zones);
                        ImGui::Indent();
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    ImGui::SliderFloat("Worley Cell Number", &imgui.worley_cell_nb, 1.0f,
                       10.0f);
    ImGui::SliderInt("Octaves", &imgui.octaves, 1, 8);
    ImGui::SliderFloat("Noise Threshold", &imgui.threshold, 0.0f, 1.0f);
    if (ImGui::Button("Reload texture"))
    {
        imgui.generate_texture = true;
    }
}

std::unique_ptr<Scene> create_default_scene()
{
    auto scene = std::make_unique<Scene>();

    // Load default cube model
    auto result = Scene::from_gltf(std::string(data_path) + "cube.glb");
    ALWAYS_ASSERT(result.is_ok, "Unable to load default scene");
    scene = std::move(result.value);

    auto sphere = Scene::from_gltf(std::string(data_path) + "sphere.glb");
    scene->add_object(sphere.value->objects()[0]);

    scene->set_sun(glm::vec3(0.2f, 1.0f, 0.1f), glm::vec3(1.0f));

    // Add lights
    {
        PointLight light;
        // light.set_position(glm::vec3(1.0f, 2.0f, 4.0f));
        // light.set_position(glm::vec3(2.0f, 15.0f, 2.0f));
        light.set_position(glm::vec3(-0.315f, 0.719f, 0.6187f));
        // light.set_color(glm::vec3(0.0f, 50.0f, 0.0f));
        light.set_color(glm::vec3(20.0f, 20.0f, 20.0f));
        light.set_radius(100.0f);
        // light.set_radius(4.0f);
        scene->add_light(std::move(light));
    }
    {
        PointLight light;
        light.set_position(glm::vec3(1.0f, 2.0f, -4.0f));
        light.set_color(glm::vec3(50.0f, 0.0f, 0.0f));
        // light.set_radius(4.0f);
        light.set_radius(50.0f);
        scene->add_light(std::move(light));
    }

    return scene;
}

struct RendererState
{
    static RendererState create(glm::uvec2 size)
    {
        RendererState state;

        state.size = size;

        if (state.size.x > 0 && state.size.y > 0)
        {
            state.depth_texture = Texture(size, ImageFormat::Depth32_FLOAT);
            state.lit_hdr_texture = Texture(size, ImageFormat::RGBA16_FLOAT);
            state.tone_mapped_texture = Texture(size, ImageFormat::RGBA8_UNORM);
            state.g_albedo_texture = Texture(size, ImageFormat::RGBA8_sRGB);
            state.g_normal_texture = Texture(size, ImageFormat::RGBA8_UNORM);
            state.g_debug_texture = Texture(size, ImageFormat::RGBA16_FLOAT);
            state.cloud_texture = Texture(size, ImageFormat::RGBA8_UNORM);

            state.main_framebuffer = Framebuffer(
                &state.depth_texture, std::array{ &state.lit_hdr_texture });

            state.tone_map_framebuffer =
                Framebuffer(nullptr, std::array{ &state.tone_mapped_texture });

            state.z_prepass_framebuffer = Framebuffer(&state.depth_texture);
            state.g_framebuffer = Framebuffer(
                &state.depth_texture,
                std::array{ &state.g_albedo_texture, &state.g_normal_texture });

            state.g_debug_framebuffer =
                Framebuffer(nullptr, std::array{ &state.g_debug_texture });

            state.cloud_framebuffer =
                Framebuffer(nullptr, std::array{ &state.cloud_texture });

        }

        return state;
    }

    glm::uvec2 size = {};

    Texture depth_texture;
    Texture lit_hdr_texture;
    Texture tone_mapped_texture;

    Framebuffer main_framebuffer;
    Framebuffer tone_map_framebuffer;

    // Z Prepass
    Framebuffer z_prepass_framebuffer;

    // G Buffer
    Framebuffer g_framebuffer;
    Texture g_albedo_texture;
    Texture g_normal_texture;

    Framebuffer g_debug_framebuffer;
    Texture g_debug_texture;

    // Volumetric
    Framebuffer cloud_framebuffer;
    Texture cloud_texture;
};

int main(int argc, char** argv)
{
    DEBUG_ASSERT([] {
        std::cout << "Debug asserts enabled" << std::endl;
        return true;
    }());

    parse_args(argc, argv);

    glfw_check(glfwInit());
    DEFER(glfwTerminate());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "OM3D", nullptr, nullptr);
    glfw_check(window);
    DEFER(glfwDestroyWindow(window));

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    init_graphics();

    ImGuiRenderer imgui(window);

    scene = create_default_scene();

    std::cout << "Scene has " << scene->objects().size() << " objects\n";

    auto tonemap_program = Program::from_files("tonemap.frag", "screen.vert");
    auto g_debug_program = Program::from_files("g_debug.frag", "screen.vert");
    auto g_global_illumination_program =
        Program::from_files("g_global_illumination.frag", "screen.vert");
    auto g_local_illumination_program =
        Program::from_files("g_local_illumination.frag", "basic.vert");

    auto noise_program = Program::from_file("test_noise.comp");
    auto erosion_program = Program::from_file("erosion.comp");
    auto cloud_program = Program::from_file("clouds.comp");

    auto light_material = Material::empty_material();
    light_material->set_program(g_local_illumination_program);
    light_material->set_blend_mode(BlendMode::Additive);
    light_material->set_depth_test_mode(DepthTestMode::Reversed);

    auto cloud_material = Material::empty_material();
    cloud_material->set_program(cloud_program);
    cloud_material->set_blend_mode(BlendMode::None);
    cloud_material->set_depth_test_mode(DepthTestMode::None);

    auto sphere = scene->objects()[1];
    SceneObject cloudSphere = sphere;

    sphere.set_material(light_material);
    cloudSphere.set_material(cloud_material);

    RendererState renderer;

    int noiseSize = 512;
    Texture3D noise_texture = Texture3D(glm::uvec3(noiseSize, noiseSize, noiseSize), ImageFormat::RGBA8_UNORM);
    Texture weather_texture = Texture(glm::uvec2(noiseSize, noiseSize), ImageFormat::RGBA8_UNORM);
    Texture weather_texture_intermediary = Texture(glm::uvec2(noiseSize, noiseSize), ImageFormat::RGBA8_UNORM);
    Framebuffer noise_framebuffer = Framebuffer(nullptr, std::array{ &weather_texture_intermediary });

    for (;;)
    {
        glfwPollEvents();
        if (glfwWindowShouldClose(window)
            || glfwGetKey(window, GLFW_KEY_ESCAPE))
        {
            break;
        }

        process_profile_markers();

        {
            int width = 0;
            int height = 0;
            glfwGetWindowSize(window, &width, &height);

            if (renderer.size != glm::uvec2(width, height))
            {
                renderer = RendererState::create(glm::uvec2(width, height));
            }
        }

        update_delta_time();

        if (const auto& io = ImGui::GetIO();
            !io.WantCaptureMouse && !io.WantCaptureKeyboard)
        {
            process_inputs(window, scene->camera());
        }

        // Draw everything
        {
            PROFILE_GPU("Frame");

            // Z prepass
            // {
            //     PROFILE_GPU("Z pass");
            //     renderer.z_prepass_framebuffer.bind(true, false);
            //     scene->render();
            // }

            // Render the scene
            // {
            //     PROFILE_GPU("Main pass");
            //
            //     renderer.g_framebuffer.bind(false, true);
            //     scene->render();
            // }

            // Tries with noise
            if (imgui.generate_texture)
            {
                imgui.generate_texture = false;
                std::cout << "Reloading texture" << std::endl;
                PROFILE_GPU("Noise Generation");

                noise_program->bind();
                // noise_framebuffer.bind(true, true);

                noise_program->set_uniform(HASH("worley_cell_nb"),
                                           imgui.worley_cell_nb);
                noise_program->set_uniform(HASH("threshold"), imgui.threshold);
                noise_program->set_uniform(HASH("size"), u32(noiseSize));

                u32 octaves = imgui.octaves;
                noise_program->set_uniform(HASH("octaves"), octaves);

                noise_texture.bind_as_image(0, AccessType::WriteOnly);
                weather_texture_intermediary.bind_as_image(1, AccessType::WriteOnly);

                // Size of the noise texture
                // glDispatchCompute(noiseSize, noiseSize, noiseSize);
                // glMemoryBarrier(GL_ALL_BARRIER_BITS);
                //
                // erosion_program->bind();
                // erosion_program->set_uniform(HASH("size"), u32(noiseSize));
                //
                // weather_texture_intermediary.bind_as_image(0, AccessType::ReadOnly);
                // weather_texture.bind_as_image(1, AccessType::WriteOnly);
                //
                // glDispatchCompute(noiseSize / 16, noiseSize / 16, 1);
                // glMemoryBarrier(GL_ALL_BARRIER_BITS);
            }

            // Render the clouds
            {
                // For now, assuming the cloud pass happens after the
                // Illumination part. Anyway, since we will focus solely on the
                // cloud at the beginning, not a real issue for now.
                PROFILE_GPU("Clouds pass");

                cloud_program->bind();
                renderer.cloud_framebuffer.bind(true, true);

                int width = 0;
                int height = 0;
                glfwGetWindowSize(window, &width, &height);

                // TO PUT IN CLOUD DATA
                cloud_program->set_uniform(HASH("direction"),
                                           scene->camera().forward());
                cloud_program->set_uniform(HASH("up"), scene->camera().up());
                cloud_program->set_uniform(HASH("fov"), scene->camera().fov());
                cloud_program->set_uniform(HASH("threshold"), imgui.threshold);
                cloud_program->set_uniform(HASH("worley_cell_nb"),
                                           imgui.worley_cell_nb);
                cloud_program->set_uniform(HASH("sun_debug"),
                                           sun_debug ? u32(1) : u32(0));

                cloud_program->set_uniform(HASH("g0"), g0);

                cloud_program->set_uniform(HASH("g1"), g1);

                cloud_program->set_uniform(HASH("w"), w);

                cloud_program->set_uniform(HASH("sigma_a"), sigma_a);
                cloud_program->set_uniform(HASH("sigma_s"), sigma_s);

                cloud_program->set_uniform(HASH("step_size"), step_size);

                cloud_program->set_uniform(
                    HASH("resolution"),
                    glm::vec2(static_cast<float>(width),
                              static_cast<float>(height)));

                TypedBuffer<shader::CloudData> buffer(nullptr, 1);
                {
                    auto mapping = buffer.map(AccessType::WriteOnly);
                    mapping[0].camera.view_proj = scene->view_proj_matrix();
                    mapping[0].camera.camera_pos = scene->camera().position();
                    // mapping[0].resolution.x = ;
                    // mapping[0].resolution.y = ;
                }
                buffer.bind(BufferUsage::Uniform, 0);

                TypedBuffer<shader::PointLight> light_buffer(
                    nullptr, std::max(scene->point_lights().size(), size_t(1)));

                auto mapping = light_buffer.map(AccessType::WriteOnly);
                mapping[0] = { light_pos, 1000,
                               glm::vec3(1.0, 1.0, 1.0) * light_intensity,
                               0.0f };

                light_buffer.bind(BufferUsage::Storage, 1);

                // renderer.g_albedo_texture.bind(0);
                // renderer.depth_texture.bind(1);

                renderer.cloud_texture.bind_as_image(0, AccessType::WriteOnly);
                noise_texture.bind(1);
                weather_texture.bind(2);

                glDispatchCompute(width, height, 1);
                glMemoryBarrier(GL_ALL_BARRIER_BITS);

                // const auto& camera = scene->camera();
                // const auto& frustum = camera.build_frustum();
                //
                // cloudSphere.render(camera, frustum);
            }

            // if (imgui._debug_texture < 3)
            // {
            //     PROFILE_GPU("G buffer debug");
            //
            //     glDisable(GL_CULL_FACE);
            //
            //     renderer.g_debug_framebuffer.bind(true, true);
            //
            //     g_debug_program->bind();
            //     g_debug_program->set_uniform(HASH("texture"),
            //                                  imgui._debug_texture);
            //     renderer.g_albedo_texture.bind(0);
            //     renderer.g_normal_texture.bind(1);
            //     renderer.depth_texture.bind(2);
            //     glDrawArrays(GL_TRIANGLES, 0, 3);
            // }
            // else
            // {
            //     {
            //         PROFILE_GPU("Global Illumination");
            //
            //         glDisable(GL_CULL_FACE);
            //         renderer.g_debug_framebuffer.bind(true, true);
            //
            //         g_global_illumination_program->bind();
            //         TypedBuffer<shader::FrameData> buffer(nullptr, 1);
            //         {
            //             auto mapping = buffer.map(AccessType::WriteOnly);
            //             mapping[0].camera.view_proj =
            //             scene->view_proj_matrix();
            //             mapping[0].camera.camera_pos =
            //             scene->camera().position();
            //             mapping[0].point_light_count =
            //                 scene->point_lights().size();
            //             mapping[0].sun_color = scene->get_sun_color();
            //             mapping[0].sun_dir = scene->get_sun_direction();
            //         }
            //         buffer.bind(BufferUsage::Uniform, 0);
            //
            //         renderer.g_albedo_texture.bind(0);
            //         renderer.g_normal_texture.bind(1);
            //         renderer.depth_texture.bind(2);
            //         glDrawArrays(GL_TRIANGLES, 0, 3);
            //     }
            //     {
            //         PROFILE_GPU("Local Illumination");
            //
            //         // Culling is handled in the material binding
            //         renderer.g_debug_framebuffer.bind(false, false);
            //         light_material->bind();
            //
            //         // Fill and bind lights buffer
            //         TypedBuffer<shader::FrameData> buffer(nullptr, 1);
            //         {
            //             auto mapping = buffer.map(AccessType::WriteOnly);
            //             mapping[0].camera.view_proj =
            //             scene->view_proj_matrix();
            //             mapping[0].camera.camera_pos =
            //             scene->camera().position();
            //             mapping[0].point_light_count =
            //                 scene->point_lights().size();
            //         }
            //         buffer.bind(BufferUsage::Uniform, 0);
            //
            //         TypedBuffer<shader::PointLight> light_buffer(
            //             nullptr,
            //             std::max(scene->point_lights().size(), size_t(1)));
            //         {
            //             auto mapping =
            //             light_buffer.map(AccessType::WriteOnly); for (size_t
            //             i = 0; i != scene->point_lights().size();
            //                  ++i)
            //             {
            //                 const auto light = scene->point_lights()[i];
            //                 mapping[i] = { light.position(), light.radius(),
            //                                light.color(), 0.0f };
            //             }
            //         }
            //         light_buffer.bind(BufferUsage::Storage, 1);
            //
            //         renderer.g_albedo_texture.bind(0);
            //         renderer.g_normal_texture.bind(1);
            //         renderer.depth_texture.bind(2);
            //
            //         const auto& camera = scene->camera();
            //         const auto& frustum = camera.build_frustum();
            //         g_local_illumination_program->set_uniform(
            //             HASH("wireframe"),
            //             static_cast<u32>(imgui._debug_texture == 4));
            //         for (size_t i = 0; i < scene->point_lights().size(); i++)
            //         {
            //             const auto light = scene->point_lights()[i];
            //
            //             bool to_draw =
            //                 in_plane(frustum._left_normal, camera.position(),
            //                          light.position(), light.radius());
            //             to_draw &=
            //                 in_plane(frustum._top_normal, camera.position(),
            //                          light.position(), light.radius());
            //             to_draw &=
            //                 in_plane(frustum._right_normal,
            //                 camera.position(),
            //                          light.position(), light.radius());
            //             to_draw &=
            //                 in_plane(frustum._bottom_normal,
            //                 camera.position(),
            //                          light.position(), light.radius());
            //             to_draw &=
            //                 in_plane(frustum._near_normal, camera.position(),
            //                          light.position(), light.radius());
            //
            //             if (!to_draw)
            //             {
            //                 continue;
            //             }
            //
            //             light_material->set_uniform(HASH("light_id"),
            //                                         static_cast<OM3D::u32>(i));
            //
            //             sphere.set_transform(
            //                 glm::translate(glm::mat4(1.0f), light.position())
            //                 * glm::scale(glm::mat4(1.0f),
            //                              glm::vec3(light.radius())));
            //
            //             if (imgui._debug_texture == 4)
            //                 glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            //
            //             sphere.render(camera, frustum);
            //
            //             if (imgui._debug_texture == 4)
            //                 glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            //         }
            //     }
            // Apply a tonemap in compute shader
            //     {
            //         PROFILE_GPU("Tonemap");
            //
            //         glDisable(GL_CULL_FACE);
            //         glDisable(GL_BLEND);
            //         renderer.tone_map_framebuffer.bind(false, true);
            //         tonemap_program->bind();
            //         tonemap_program->set_uniform(HASH("exposure"), exposure);
            //         renderer.g_debug_texture.bind(0);
            //         glDrawArrays(GL_TRIANGLES, 0, 3);
            //     }
            // }

            // Blit tonemap result to screen
            {
                PROFILE_GPU("Blit");

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (imgui._debug_texture == 5)
                    noise_framebuffer.blit();
                else
                    renderer.cloud_framebuffer.blit();
                // if (imgui._debug_texture == 3)
                //     renderer.tone_map_framebuffer.blit();
                // else
                //     renderer.g_debug_framebuffer.blit();
            }

            // Draw GUI on top
            gui(imgui);
        }

        glfwSwapBuffers(window);
    }

    scene = nullptr; // destroy scene and child OpenGL objects
}
