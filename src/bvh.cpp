#include "bvh.h"
#include "draw.h"
#include "interpolate.h"
#include "intersect.h"
#include "render.h"
#include "scene.h"
#include <stack>

void updateHitInfo(const Scene& scene, const BVHInterface::Primitive& primitive, const Ray& ray, HitInfo& hitInfo)
{
    const auto& [v0, v1, v2] = std::tie(primitive.v0, primitive.v1, primitive.v2);
    const auto& mesh = scene.meshes[primitive.meshID];
    const auto n = glm::normalize(glm::cross(v1.position - v0.position, v2.position - v0.position));

    hitInfo.material = mesh.material;
    hitInfo.normal = n;

    if (glm::dot(ray.direction, n) > 0.0f) {
        hitInfo.normal = -hitInfo.normal;
    }
}

bool intersectRayWithAABB(AxisAlignedBox aabb, const Ray& ray)
{
    float min = 0.0f;
    float max = ray.t;

    for (int i = 0; i < 3; i++) {
        float x = 1.0f / ray.direction[i];
        float a = (aabb.lower[i] - ray.origin[i]) * x;
        float b = (aabb.upper[i] - ray.origin[i]) * x;

        if (x < 0.0f) {
            std::swap(a, b);
        }

        min = std::max(min, a);
        max = std::min(max, b);
        {
            if (max < min)
                return false;
        }
    }
    return true;
}

// TODO Standard Feature
// Hierarchy traversal routine; you must implement this method and implement it carefully!
//
// The default implementation uses precompiled methods and only performs rudimentary updates to hitInfo.
// For correct normal interpolation, barycentric coordinates, etc., you have to implement this traversal yourself.
// If you have implemented the traversal method for assignment 4B, you might be able to reuse most of it.
//
// This method returns `true` if geometry was hit, and `false` otherwise. On first/closest hit, the
// distance `t` in the `ray` object is updated, and information is updated in the `hitInfo` object.
// - state;    current render state (containing scene, features, ...)
// - bvh;      the actual bvh which should be traversed for faster intersection
// - ray;      the ray intersecting the scene's geometry
// - hitInfo;  the return object, with info regarding the hit geometry
// - return;   boolean, if geometry was hit or not
bool BVH::intersect(RenderState& state, Ray& ray, HitInfo& hitInfo) const
{
    std::stack<Node> stack;
    stack.push(state.bvh.nodes()[BVH::RootIndex]);
    float color = 0.95f;
    bool hit = false;

    while (!stack.empty()) {
        Node node = stack.top();
        stack.pop();

        if (state.features.enableDebugDraw && state.features.enableAccelStructure) {
            drawAABB(node.aabb, DrawMode::Wireframe, intersectRayWithAABB(node.aabb, ray) ? glm::vec3(1, color, color) : glm::vec3(1, 1, 1), 1.0f);
        }

        if (!intersectRayWithAABB(node.aabb, ray)) {
            continue;
        }

        if (state.features.enableDebugDraw && state.features.enableAccelStructure) {
            color *= 0.93f;
        }

        if (node.isLeaf()) {
            int offset = node.primitiveOffset();
            int count = node.primitiveCount();

            for (int i = 0; i < count; ++i) {
                Primitive primitive = state.bvh.primitives()[offset + i];

                if (intersectRayWithTriangle(primitive.v0.position, primitive.v1.position, primitive.v2.position, ray, hitInfo)) {
                    if (state.features.enableDebugDraw && state.features.enableAccelStructure) {
                        drawTriangle(primitive.v0, primitive.v1, primitive.v2);
                        drawSphere(ray.origin + ray.t * ray.direction, 0.01f, { 1, 1, 0 });
                    }

                    hit = true;

                    glm::vec3 intersectionPoint = ray.t * ray.direction + ray.origin;
                    glm::vec3 bari = computeBarycentricCoord(primitive.v0.position, primitive.v1.position, primitive.v2.position, intersectionPoint);

                    hitInfo.normal = interpolateNormal(primitive.v0.normal, primitive.v1.normal, primitive.v2.normal, bari);
                    hitInfo.texCoord = interpolateTexCoord(primitive.v0.texCoord, primitive.v1.texCoord, primitive.v2.texCoord, bari);
                    hitInfo.barycentricCoord = bari;
                    hitInfo.material = state.scene.meshes[primitive.meshID].material;
                }
            }
        } else {
            stack.push(state.bvh.nodes()[node.leftChild()]);
            stack.push(state.bvh.nodes()[node.rightChild()]);
        }
    }

    return hit;
}
