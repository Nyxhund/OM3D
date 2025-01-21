#include "Texture3D.h"

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <algorithm>
#include <cmath>
#include <stb/stb_image.h>

namespace OM3D
{

    static GLuint create_texture_handle()
    {
        GLuint handle = 0;
        glCreateTextures(GL_TEXTURE_3D, 1, &handle);
        return handle;
    }

    Texture3D::Texture3D(const glm::uvec2& size, ImageFormat format)
    {
        _handle = GLHandle(create_texture_handle());
        _size = size;
        _format = format;

        const ImageFormatGL gl_format = image_format_to_gl(_format);
        glTextureStorage3D(_handle.get(), 1, gl_format.internal_format, _size.x,
                           _size.y, 127);

        if (bindless_enabled())
        {
            _bindless = glGetTextureHandleARB(_handle.get());
            glMakeTextureHandleResidentARB(_bindless);
        }
    }

    Texture3D::~Texture3D()
    {
        if (auto handle = _handle.get())
        {
            glDeleteTextures(1, &handle);
        }
    }

    void Texture3D::bind_as_image(u32 index, AccessType access)
    {
        glBindImageTexture(index, _handle.get(), 0, true, 0,
                           access_type_to_gl(access),
                           image_format_to_gl(_format).internal_format);
    }

} // namespace OM3D
