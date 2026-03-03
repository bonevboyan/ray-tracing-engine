#include "texture.h"
#include "render.h"
#include <framework/image.h>
#include <fmt/core.h>
#include <glm/gtx/string_cast.hpp>

// TODO: Standard feature
// Given an image, and relevant texture coordinates, sample the texture s.t.
// the nearest texel to the coordinates is acquired from the image.
// - image;    the image object to sample from.
// - texCoord; sample coordinates, generally in [0, 1]
// - return;   the nearest corresponding texel
// This method is unit-tested, so do not change the function signature.
glm::vec3 sampleTextureNearest(const Image& image, const glm::vec2& texCoord)
{
    // TODO: implement this function.
    // Note: the pixels are stored in a 1D array, row-major order. You can convert from (i, j) to
    //       an index using the method seen in the lecture.
    // Note: the center of the first pixel should be at coordinates (0.5, 0.5)
    // Given texcoords, return the corresponding pixel of the image
    // The pixel are stored in a 1D array of row major order
    // you can convert from position (i,j) to an index using the method seen in the lecture
    // Note, the center of the first pixel is at image coordinates (0.5, 0.5)
    int i = std::floor(texCoord.x * image.width);
    int j = std::floor(texCoord.y * image.height);
    const int idx = j * image.width + i;
    return image.get_pixel(idx);
}

// TODO: Standard feature
// Given an image, and relevant texture coordinates, sample the texture s.t.
// a bilinearly interpolated texel is acquired from the image.
// - image;    the image object to sample from.
// - texCoord; sample coordinates, generally in [0, 1]
// - return;   the filter of the corresponding texels
// This method is unit-tested, so do not change the function signature.
glm::vec3 sampleTextureBilinear(const Image& image, const glm::vec2& texCoord)
{
    // TODO: implement this function.
    // Note: the pixels are stored in a 1D array, row-major order. You can convert from (i, j) to
    //       an index using the method seen in the lecture.
    // Note: the center of the first pixel should be at coordinates (0.5, 0.5)
    // Given texcoords, return the corresponding pixel of the image
    // The pixel are stored in a 1D array of row major order
    // you can convert from position (i,j) to an index using the method seen in the lecture
    // Note, the center of the first pixel is at image coordinates (0.5, 0.5)

    const float x = texCoord.x * image.width;
    const float y = texCoord.y * image.height;

    int x0 = int(std::floor(x));
    int y0 = int(std::floor(y));
    int x1 = (x - x0 > 0.5f) ? glm::clamp(x0 + 1, 0, image.width - 1) : glm::clamp(x0 - 1, 0, image.width - 1);
    int y1 = (y - y0 > 0.5f) ? glm::clamp(y0 + 1, 0, image.height - 1) : glm::clamp(y0 - 1, 0, image.height - 1);

    if (x0 > x1) {
        std::swap(x0, x1);
    }
    if (y0 > y1) {
        std::swap(y0, y1);
    }
    const float fx = x - x0 -0.5f;
    const float fy = y - y0 - 0.5f;

    const glm::vec3 c00 = image.get_pixel(y0 * image.width + x0);
    const glm::vec3 c10 = image.get_pixel(y0 * image.width + x1);
    const glm::vec3 c01 = image.get_pixel(y1 * image.width + x0);
    const glm::vec3 c11 = image.get_pixel(y1 * image.width + x1);

    const glm::vec3 c0 = c00 * (1.0f - fx) + c10 * fx;
    const glm::vec3 c1 = c01 * (1.0f - fx) + c11 * fx;

    const glm::vec3 res = c0 * (1.0f - fy) + c1 * fy;

    return res;
}