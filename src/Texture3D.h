#ifndef TEXTURE3D_H
#define TEXTURE3D_H

#include "Texture.h"

namespace OM3D
{

    class Texture3D : public Texture
    {
    public:
        Texture3D() = default;
        Texture3D(Texture3D&&) = default;
        Texture3D& operator=(Texture3D&&) = default;
        Texture3D(const glm::uvec2& size, ImageFormat format);

        virtual ~Texture3D();

        virtual void bind_as_image(u32 index, AccessType access);
    };

} // namespace OM3D

#endif // TEXTURE3D_H
