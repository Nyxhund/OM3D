#include "Scene.h"

#include <TypedBuffer.h>
#include <shader_structs.h>
#include <iostream>

#include "Camera.h"

namespace OM3D
{

    Scene::Scene()
    {}

    void Scene::add_object(SceneObject obj)
    {
        _objects.emplace_back(std::move(obj));
    }

    void Scene::add_light(PointLight obj)
    {
        // obj.set_position(glm::vec3(1.0f, 1.0f, 1.0f));
        // obj.set_radius(0.5f);
        // std::cout << "Color is " << obj.color().r << ", " << obj.color().g << ", " << obj.color().b << "\n";
        // obj.set_color(glm::vec3(0.0f, 50.0f, 0.0f));
        _point_lights.emplace_back(std::move(obj));
    }

    const glm::vec3& Scene::get_sun_direction() const
    {
        return _sun_direction;
    }

    const glm::vec3& Scene::get_sun_color() const
    {
        return _sun_color;
    }

    Span<const SceneObject> Scene::objects() const
    {
        return _objects;
    }

    Span<const PointLight> Scene::point_lights() const
    {
        return _point_lights;
    }

    Camera& Scene::camera()
    {
        return _camera;
    }

    const Camera& Scene::camera() const
    {
        return _camera;
    }

    void Scene::set_sun(glm::vec3 direction, glm::vec3 color)
    {
        _sun_direction = direction;
        _sun_color = color;
    }

    void Scene::render(bool z_prepass) const
    {
        // Fill and bind frame data buffer
        TypedBuffer<shader::FrameData> buffer(nullptr, 1);
        {
            auto mapping = buffer.map(AccessType::WriteOnly);
            mapping[0].camera.view_proj = _camera.view_proj_matrix();
            mapping[0].point_light_count = u32(_point_lights.size());
            mapping[0].sun_color = _sun_color;
            mapping[0].sun_dir = glm::normalize(_sun_direction);
        }
        buffer.bind(BufferUsage::Uniform, 0);

        if (!z_prepass)
        {
            // Fill and bind lights buffer
            TypedBuffer<shader::PointLight> light_buffer(
                nullptr, std::max(_point_lights.size(), size_t(1)));
            {
                auto mapping = light_buffer.map(AccessType::WriteOnly);
                for (size_t i = 0; i != _point_lights.size(); ++i)
                {
                    const auto& light = _point_lights[i];
                    mapping[i] = { light.position(), light.radius(),
                                   light.color(), 0.0f };
                }
            }
            light_buffer.bind(BufferUsage::Storage, 1);
        }

        const Frustum f = _camera.build_frustum();
        // Render every object
        for (const SceneObject& obj : _objects)
        {
            obj.render(_camera, f);
        }
    }

    const glm::mat4& Scene::view_proj_matrix() const
    {
        return _camera.view_proj_matrix();
    }

} // namespace OM3D
