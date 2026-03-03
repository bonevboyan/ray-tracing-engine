#include "extra.h"
#include "bvh.h"
#include "intersect.h"
#include "light.h"
#include "recursive.h"
#include "shading.h"
#include "texture.h"
#include "draw.h"
#include <framework/trackball.h>

#include <numeric>
#include "interpolate.h"
#include "texture.h"
#include <intersect.h>
#include "draw.h"

// TODO; Extra feature
// Given the same input as for `renderImage()`, instead render an image with your own implementation
// of Depth of Field. Here, you generate camera rays s.t. a focus point and a thin lens camera model
// are in play, allowing objects to be in and out of focus.
// This method is not unit-tested, but we do expect to find it **exactly here**, and we'd rather
// not go on a hunting expedition for your implementation, so please keep it here!
void renderImageWithDepthOfField(const Scene& scene, const BVHInterface& bvh, const Features& features, const Trackball& camera, Screen& screen)
{
    if (!features.extra.enableDepthOfField) {
        return;
    }

    float lensRadius = 0.1f;
    float focalLength = 2.7f;
    int samples = 16;

#ifdef NDEBUG // Enable multi threading in Release mode
#pragma omp parallel for schedule(guided)
#endif
    for (int y = 0; y < screen.resolution().y; y++) {
        for (int x = 0; x < screen.resolution().x; x++) {
            RenderState state = {
                .scene = scene,
                .features = features,
                .bvh = bvh,
                .sampler = { static_cast<uint32_t>(screen.resolution().y * x + y) }
            };

            Ray ray = generatePixelRays(state, camera, { x, y }, screen.resolution()).front();

            float t = focalLength / glm::dot(ray.direction, camera.forward());
            glm::vec3 focalPoint = ray.origin + t * ray.direction;

            glm::vec3 colorSum { 0, 0, 0 };

            for (int i = 0; i < samples; ++i) {
                glm::vec2 random;
                while (true) {
                    glm::vec2 point = state.sampler.next_2d();
                    if (glm::dot(point, point) <= 1.0f) {
                        random = point;
                        break;
                    }
                }
                glm::vec2 lensSample = lensRadius * random;
                glm::vec3 samplePoint = camera.position() 
                    + lensSample.x * camera.left() 
                    + lensSample.y * camera.up();
                glm::vec3 sampleDirection = glm::normalize(focalPoint - samplePoint);

                std::vector<Ray> rays;
                Ray sampleRay(samplePoint, sampleDirection);
                rays.emplace_back(sampleRay);

                colorSum += renderRays(state, rays);
            }

            screen.setPixel(x, y, colorSum / static_cast<float>(samples));
        }
    }
}


glm::vec3 bezierPoint(const std::vector<glm::vec3> splinePoints, const glm::vec3& pos, float time)
{
    // resource: https://en.wikipedia.org/wiki/B%C3%A9zier_curve
    std::vector<glm::vec3> P = { splinePoints[0] , splinePoints[1] , splinePoints[2] , splinePoints[3]  };

    P[0] *= std::pow((1 - time), 3);
    P[1] *= 3 * std::pow((1 - time), 2) * time;
    P[2] *= 3 * std::pow((1 - time), 1) * std::pow(time, 2);
    P[3] *= std::pow(time, 3);

    return P[0] + P[1] + P[2] + P[3];
}


void renderSpline(const std::vector<glm::vec3> splinePoints, const glm::vec3 pos)
{

    for (int i = 0; i <= 1000; i++) {
        float time = i * 1.0f / 1000;
        glm::vec3 currentPoint = bezierPoint(splinePoints, pos, time);
        Material material = { .kd = { 0.5f, 0.0f, 0.5f } };
        Sphere point = { .center = currentPoint, .radius = 0.005f, .material = material };
        drawSphere(point);
    }
}

std::vector<Mesh> updateMeshes(std::vector<Mesh> meshes, Features features, float time)
{
    std::vector<Mesh> newMeshes;
    for (int i = 0; i < meshes.size(); i++) {
        Mesh m = meshes[i];
        std::vector<Vertex> vertices;
        for (int j = 0; j < m.vertices.size(); j++) {
            Vertex v = m.vertices[j];
            Vertex newVertex = { .position = v.position + bezierPoint(features.extra.splinePoints, v.position, time), .normal = v.normal, .texCoord = v.texCoord };
            vertices.push_back(newVertex);
        }
        Mesh newMesh = { .vertices = vertices, .triangles = m.triangles, .material = m.material };
        newMeshes.push_back(newMesh);
    }
    return newMeshes;
}


std::vector<Sphere> updateSpheres(std::vector<Sphere> spheres, Features features, float time)
{
    std::vector<Sphere> newSpheres;
    for (int i = 0; i < spheres.size(); i++) {
        Sphere s = spheres[i];
        Sphere newSphere = { .center = s.center + bezierPoint(features.extra.splinePoints, s.center, time), .radius = s.radius, .material = s.material };
        newSpheres.push_back(newSphere);
    }
    return newSpheres;
}


void renderSceneAtTime(const Scene& scene, const Features& features, const Trackball& camera, Screen& screen, float time)
{
    Scene newScene = { .type = scene.type, .meshes = updateMeshes(scene.meshes, features, time), .spheres = updateSpheres(scene.spheres, features, time), .lights = scene.lights };
    RenderState newState = { .scene = newScene,
        .features = features,
        .bvh = BVH(newScene, features),
        .sampler = { 0 } };

    for (int y = 0; y < screen.resolution().y; y++) {
        for (int x = 0; x < screen.resolution().x; x++) {
            screen.setPixel(x, y, renderRays(newState, generatePixelRays(newState, camera, { x, y }, screen.resolution())));
        }
    }
}



// TODO; Extra feature
// Given the same input as for renderImage(), instead render an image with your own implementation
// of motion blur. Here, you integrate over a time domain, and not just the pixel's image domain,
// to give objects the appearance of "fast movement".
// This method is not unit-tested, but we do expect to find it **exactly here**, and we'd rather
// not go on a hunting expedition for your implementation, so please keep it here!
void renderImageWithMotionBlur(const Scene& scene, const BVHInterface& bvh, const Features& features, const Trackball& camera, Screen& screen)
{
    if (!features.extra.enableMotionBlur) {
        return;
    }

#ifdef NDEBUG
#pragma omp parallel for schedule(guided)
#endif
    for (int y = 0; y < screen.resolution().y; y++) {
        for (int x = 0; x < screen.resolution().x; x++) {

            RenderState state = { .scene = scene,
                .features = features,
                .bvh = bvh,
                .sampler = { static_cast<uint32_t>(screen.resolution().y * x + y) } };

            glm::vec3 pixel = { 0.0f, 0.0f, 0.0f };

            for (int i = 0; i < features.extra.numBlurSamples; i++) {
                float time = i * 1.0f / features.extra.numBlurSamples;

                Scene newScene = { .type = scene.type, .meshes = updateMeshes(scene.meshes, features, time), .spheres = updateSpheres(scene.spheres, features, time), .lights = scene.lights };
                RenderState newState = { .scene = newScene,
                    .features = features,
                    .bvh = BVH(newScene, features),
                    .sampler = state.sampler };

                pixel += renderRays(newState, generatePixelRays(newState, camera, { x, y }, screen.resolution()));
            }

            pixel /= features.extra.numBlurSamples * 1.0f;
            screen.setPixel(x, y, pixel);
        }
    }
}

// need combinations
int C(int n, int k)
{
    if (n == k || k <= 0) {
        return 1;
    }
    if (2 * k > n) {
        k = n - k;
    }
    int ret = n;
    for (int i = k; i > 1; --i) {
        ret = ret * (n - i + 1);
        ret /= i;
    }
    return ret;
}

// Extra feature
// Given a rendered image, compute and apply a bloom post-processing effect to increase bright areas.
// This method is not unit-tested, but we do expect to find it **exactly here**, and we'd rather
// not go on a hunting expedition for your implementation, so please keep it here!
void postprocessImageWithBloom(const Scene& scene, const Features& features, const Trackball& camera, Screen& image)
{
    if (!features.extra.enableBloomEffect) {
        return;
    }
    std::vector<glm::vec3> bright(image.resolution().x * image.resolution().y, { 0, 0, 0 });
    int filter_size = features.extra.bloom_filter_size;
    if (filter_size % 2 == 0)
        --filter_size;
    const auto& filter_scale = features.extra.bloom_filter_scale;
    // take only bright pixels
    for (int y = 0; y < image.resolution().y; ++y) {
        for (int x = 0; x < image.resolution().x; ++x) {
            const auto& col = image.pixels()[image.indexAt(x, y)];
            if (std::max({ col.x, col.y, col.z }) > features.extra.bloom_filter_threshold) {
                bright[image.indexAt(x, y)] = col;
            } else {
                bright[image.indexAt(x, y)] = glm::vec3(0.f);
            }
        }
    }
    std::vector<glm::vec3> one_d_filter(image.resolution().x * image.resolution().y, glm::vec3(0.0f));
    //filter for horizontal 1D
    for (int y = 0; y < image.resolution().y; ++y) {
        for (int x = 0; x < image.resolution().x; ++x) {
            int sum = 0;
            glm::vec3 f_color(0.f);
            for (int v = -filter_size / 2; v <= filter_size / 2; ++v) {
                if (x + v >= 0 and x + v < image.resolution().x) {
                    int n = filter_size, k = v + filter_size / 2;
                    int w = C(n - 1, k);
                    sum += w;
                    f_color += bright[image.indexAt(x + v, y)] * glm::vec3(w);
                }
            }
            one_d_filter[image.indexAt(x, y)] = f_color / glm::vec3(sum);
        }
    }
    std::vector<glm::vec3> two_d_filter(image.resolution().x * image.resolution().y, glm::vec3(0.0f));

    //filter for vertical 1D
    for (int y = 0; y < image.resolution().y; ++y) {
        for (int x = 0; x < image.resolution().x; ++x) {
            int sum = 0;
            glm::vec3 f_color(0.f);
            for (int v = -filter_size / 2; v <= filter_size / 2; ++v) {
                if (y + v >= 0 && y + v < image.resolution().y) {
                    int n = filter_size, k = v + filter_size / 2;
                    int w = C(n - 1, k);
                    sum += w;
                    f_color += one_d_filter[image.indexAt(x, y + v)] * glm::vec3(w);
                }
                two_d_filter[image.indexAt(x, y)] = f_color / glm::vec3(sum);
            }
        }
    }

    //final answer
    for (int y = 0; y < image.resolution().y; ++y) {
        for (int x = 0; x < image.resolution().x; ++x) {
            const glm::vec3& col = image.pixels()[image.indexAt(x, y)];
            if (features.enableDebugDraw) {
                if (features.extra.brightnessOnly) {
                    image.setPixel(x, y, bright[image.indexAt(x, y)]);
                } else {
                    image.setPixel(x, y,
                        two_d_filter[image.indexAt(x, y)] * glm::vec3(filter_scale));
                }
            } else {
                image.setPixel(x, y,
                    image.pixels()[image.indexAt(x, y)] + two_d_filter[image.indexAt(x, y)] * glm::vec3(filter_scale));
            }
        }
    }
}


void visualizeGlossyDisk(RenderState& state, const glm::vec3& reflectionRay, const glm::vec3& intersectionPosition, float diskSize)
{
    glm::vec3 b1, b2;
    if (abs(reflectionRay.x) > 1e-4) {
        b1 = glm::normalize(glm::vec3(-(reflectionRay.y + reflectionRay.z) / reflectionRay.x, 1, 1));
    } else if (abs(reflectionRay.y) > 1e-4) {
        b1 = glm::normalize(glm::vec3(1, -(reflectionRay.x + reflectionRay.z) / reflectionRay.y, 1));
    } else {
        b1 = glm::normalize(glm::vec3(1, 1, -(reflectionRay.y + reflectionRay.x) / reflectionRay.z));
    }
    b2 = glm::normalize(glm::cross(reflectionRay, b1));
    bool validBasis = glm::abs(glm::dot(b1, reflectionRay)) < 1e-4 && glm::abs(glm::dot(b2, reflectionRay)) < 1e-4 && glm::abs(glm::dot(b1, b2)) < 1e-4;

    if (!validBasis) {
        std::cerr << "Warning: Invalid orthogonal basis for visualization!" << std::endl;
        return;
    }
    constexpr int numSegments = 64;
    for (int i = 0; i < numSegments; ++i) {
        float angle = 2.0f * glm::pi<float>() * i / numSegments;
        glm::vec3 point = intersectionPosition + diskSize * (b1 * cos(angle) + b2 * sin(angle));
        drawSphere(point, 0.01f, { 1, 1, 1 });
    }

    for (int i = 0; i < static_cast<int>(state.features.extra.numGlossySamples); ++i) {
        float r = std::sqrt(state.sampler.next_1d()) * (diskSize / 2.0f);
        float theta = state.sampler.next_1d() * 2.0f * glm::pi<float>();
        float A = r * std::cos(theta);
        float B = r * std::sin(theta);
        glm::vec3 samplePoint = intersectionPosition + A * b1 + B * b2;
        drawSphere(samplePoint, 0.008f, { 0, 1, 0 });
    }
}

// Extra feature
// Given a camera ray (or reflected camera ray) and an intersection, evaluates the contribution of a set of
// glossy reflective rays, recursively evaluating renderRay(..., depth + 1) along each ray, and adding the
// results times material.ks to the current intersection's hit color.
// - state;    the active scene, feature config, bvh, and sampler
// - ray;      camera ray
// - hitInfo;  intersection object
// - hitColor; current color at the current intersection, which this function modifies
// - rayDepth; current recursive ray depth
// This method is not unit-tested, but we do expect to find it **exactly here**, and we'd rather
// not go on a hunting expedition for your implementation, so please keep it here!
void renderRayGlossyComponent(RenderState& state, Ray ray, const HitInfo& hitInfo, glm::vec3& hitColor, int rayDepth)
{
    
    glm::vec3 b1, b2, col(0.f);
    const glm::vec3& reflection_ray = glm::normalize(glm::reflect(ray.direction, hitInfo.normal));
    const glm::vec3& intersection_position = ray.origin + ray.direction * ray.t;
    // sample on disk orthogonal to the ray instead of quad
    const float& disk_size = hitInfo.material.shininess * 2.0f / 64.0f;

    if (state.features.enableDebugDraw && state.features.extra.visualizeGlossyDiskDebug) {
        visualizeGlossyDisk(state, reflection_ray, intersection_position, disk_size);
        return;
    }
    //find basis
    if (abs(reflection_ray.x) > 1e-4) {
        const glm::vec3& A = glm::vec3(-(reflection_ray.y + reflection_ray.z) / reflection_ray.x, 1, 1);
        b1 = glm::normalize(A);
    } else if (abs(reflection_ray.y) > 1e-4) {
        const glm::vec3& A = glm::vec3(1, -(reflection_ray.x + reflection_ray.z) / reflection_ray.y, 1);
        b1 = glm::normalize(A);
    } else {
        const glm::vec3& A = glm::vec3(1, 1, -(reflection_ray.y + reflection_ray.x) / reflection_ray.z); 
        b1 = glm::normalize(A);
   }
    b2 = glm::normalize(glm::cross(reflection_ray, b1));

    bool validBasis = glm::abs(glm::dot(b1, reflection_ray)) < 1e-4 && glm::abs(glm::dot(b2, reflection_ray)) < 1e-4 && glm::abs(glm::dot(b1, b2)) < 1e-4;

    if (!validBasis) {
        std::cout << "Wrong orthogonal basis!" << std::endl;
        return;
    }


    //parallelism?
    #ifdef NDEBUG 
    #pragma omp parallel for schedule(guided)
    #endif

    //sample disk uniformly numGlossySamples times
    for (int i = 0; i < static_cast<int>(state.features.extra.numGlossySamples); ++i) {
        HitInfo glossy_hit;
        float r = std::sqrt(state.sampler.next_1d()) * (disk_size / 2.0f);
        float theta = state.sampler.next_1d() * 2.0f * glm::pi<float>();
        float A = r * std::cos(theta);
        float B = r * std::sin(theta);
        Ray g_ray = Ray {
            .origin = intersection_position + (reflection_ray * 1e-4f),
            .direction = glm::normalize(reflection_ray + A * b1 + B * b2),
            .t = std::numeric_limits<float>::max(),
        };
        //recursively evaluate on higher depth
        col += hitInfo.material.ks * renderRay(state, g_ray, rayDepth + 1);
        if (state.features.enableDebugDraw) {
            state.bvh.intersect(state, g_ray, glossy_hit);
            drawRay(g_ray, hitColor + col / static_cast<float>(i + 1));
        }
    }
    hitColor += col / static_cast<float>(state.features.extra.numGlossySamples);
}

// Extra feature
// Given a camera ray (or reflected camera ray) that does not intersect the scene, evaluates the contribution
// along the ray, originating from an environment map. You will have to add support for environment textures
// to the Scene object, and provide a scene with the right data to supply this.
// - state; the active scene, feature config, bvh, and sampler
// - ray;   ray object
// This method is not unit-tested, but we do expect to find it **exactly here**, and we'd rather
// not go on a hunting expedition for your implementation, so please keep it here!
glm::vec3 sampleEnvironmentMap(RenderState& state, Ray ray)
{
    if (state.features.extra.enableEnvironmentMap) {
        if (!state.scene.environment_map.has_value() || !state.scene.cube_mesh.has_value())
            return glm::vec3(0.f);
        if (state.features.extra.environmentCube) {
            //Using Cube as Environment feature
            float scaleMultiplier = state.features.extra.environmentScale;
            if (abs(scaleMultiplier - 50.0f) <= 1e-4) {
                // FOR MAP AT INFINITY
                ray.origin = glm ::vec3(0.f);
            }
            HitInfo hit_info;
            bool found = false;
            Vertex A, B, C;
            for (auto indices : state.scene.cube_mesh.value().triangles) {
                const Vertex a = state.scene.cube_mesh.value().vertices[indices[0]];
                const Vertex b = state.scene.cube_mesh.value().vertices[indices[1]];
                const Vertex c = state.scene.cube_mesh.value().vertices[indices[2]];
                if (intersectRayWithTriangle(
                        a.position * scaleMultiplier,
                        b.position * scaleMultiplier,
                        c.position * scaleMultiplier,
                        ray, hit_info)) {
                    found = true;
                    A = a;
                    B = b;
                    C = c;
                }
            }
            if (!found) {
                return glm::vec3(0.f);
            }
            const auto& intersection_position = ray.origin + ray.direction * ray.t;
            const auto& barycentric_coord = computeBarycentricCoord(
                A.position * scaleMultiplier,
                B.position * scaleMultiplier,
                C.position * scaleMultiplier,
                intersection_position);
            const auto& u_v = interpolateTexCoord(A.texCoord, B.texCoord, C.texCoord, barycentric_coord);
            if (state.features.enableDebugDraw && (abs(scaleMultiplier - 50.0f) > 1e-4)) {
                const glm::vec3& sampleColor = sampleTextureNearest(state.scene.environment_map.value(), u_v);
                drawSphere(intersection_position, 0.2, sampleColor);
            }
            return sampleTextureNearest(state.scene.environment_map.value(), u_v);
        } else {
            if (state.scene.environment_sphere.has_value()) {
                const glm::vec2& coords = glm::vec2(
                    (glm::pi<float>() + atan2(ray.direction.x, ray.direction.z)) / (2 * glm::pi<float>()),
                    1 - (1 + ray.direction.y) / 2);
                const Image& image = *state.scene.environment_sphere;
                int pixelX = static_cast<int>(coords.x * image.width);
                int pixelY = static_cast<int>(coords.y * image.height);

                if (pixelX >= 0 && pixelX < image.width && pixelY >= 0 && pixelY < image.height) {
                    glm::vec3 pixel = image.get_pixel<3>(pixelY * image.width + pixelX); 
                    return pixel;
                } else {
                    return glm::vec3(0.0f);
                }
            } else {
                return glm::vec3(0.f);
            }
        }
        
    }
    return glm::vec3(0.f);
}
