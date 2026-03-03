#include "light.h"
#include "bvh_interface.h"
#include "config.h"
#include "draw.h"
#include "intersect.h"
#include "recursive.h"
#include "render.h"
#include "scene.h"
#include "shading.h"
// Suppress warnings in third-party code.
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/geometric.hpp>
DISABLE_WARNINGS_POP()

// Given a single segment light, transform a uniformly distributed 1d sample in [0, 1),
// into a uniformly sampled position and an interpolated color on the segment light,
// and write these into the reference return values.
// - sample;    a uniformly distributed 1d sample in [0, 1)
// - light;     the SegmentLight object, see `common.h`
// - position;  reference return value of the sampled position on the light
// - color;     reference return value of the color emitted by the light at the sampled position
// This method is unit-tested, so do not change the function signature.

void sampleSegmentLight(const float& sample, const SegmentLight& light, glm::vec3& position, glm::vec3& color)
{
    position = light.endpoint0 * (1 - sample) + light.endpoint1 * sample;
    color = light.color0 * (1 - sample) + light.color1 * sample;
}

// Given a single paralellogram light, transform a uniformly distributed 2d sample in [0, 1),
// into a uniformly sampled position and interpolated color on the paralellogram light,
// and write these into the reference return values.
// - sample;   a uniformly distributed 2d sample in [0, 1)
// - light;    the ParallelogramLight object, see `common.h`
// - position; reference return value of the sampled position on the light
// - color;    reference return value of the color emitted by the light at the sampled position
// This method is unit-tested, so do not change the function signature.
void sampleParallelogramLight(const glm::vec2& sample, const ParallelogramLight& light, glm::vec3& position, glm::vec3& color)
{
    glm::vec3 position1, color1;
    sampleSegmentLight(sample.x, { light.v0, light.v0 + light.edge01, light.color0, light.color1 }, position1, color1);

    glm::vec3 position2, color2;
    sampleSegmentLight(sample.x, { light.v0 + light.edge02, light.v0 + light.edge02 + light.edge01, light.color2, light.color3 }, position2, color2);

    position = position1 * (1 - sample.y) + position2 * sample.y;
    color = color1 * (1 - sample.y) + color2 * sample.y;
}

// Given a sampled position on some light, and the emitted color at this position, return whether
// or not the light is visible from the provided ray/intersection.
// For a description of the method's arguments, refer to 'light.cpp'
// - state;         the active scene, feature config, and the bvh
// - lightPosition; the sampled position on some light source
// - lightColor;    the sampled color emitted at lightPosition
// - ray;           the incident ray to the current intersection
// - hitInfo;       information about the current intersection
// - return;        whether the light is visible (true) or not (false)
// This method is unit-tested, so do not change the function signature.
bool visibilityOfLightSampleBinary(RenderState& state, const glm::vec3& lightPosition, const glm::vec3& lightColor, const Ray& ray, const HitInfo& hitInfo)
{
    const float MAX = std::numeric_limits<float>::max();
    const float EPS = 1e-2;

    if (!state.features.enableShadows)
        return true;

    if (ray.t == MAX)
        return false;

    glm::vec3 intersection = ray.origin + ray.t * ray.direction;
    Ray lightRay { lightPosition, intersection - lightPosition };
    HitInfo hInfo {};
    state.bvh.intersect(state, lightRay, hInfo);

    if (lightRay.t == MAX)
        return false;

    glm::vec3 lightIntersection = lightRay.origin + lightRay.t * lightRay.direction;
    if (glm::length(lightIntersection - intersection) >= EPS) {
        // visual debug
        if (state.scene.type != CornellBoxParallelogramLight && state.features.enableDebugDraw)
            drawRay(lightRay, { 1.0f, 0.0f, 0.0f });
        return false;
    }
    //visual debug
    if (state.scene.type != CornellBoxParallelogramLight && state.features.enableDebugDraw)
        drawRay(lightRay, { 0.0f, 1.0f, 0.0f });
    return true;
}


// Given a sampled position on some light, and the emitted color at this position, return the actual
// light that is visible from the provided ray/intersection, or 0 if this is not the case.
// Use the following blending operation: lightColor = lightColor * kd * (1 - alpha)
// Please reflect within 50 words in your report on why this is incorrect, and illustrate
// two examples of what is incorrect.
//
// - state;         the active scene, feature config, and the bvh
// - lightPosition; the sampled position on some light source
// - lightColor;    the sampled color emitted at lightPosition
// - ray;           the incident ray to the current intersection
// - hitInfo;       information about the current intersection
// - return;        the visible light color that reaches the intersection
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 visibilityOfLightSampleTransparency(RenderState& state, const glm::vec3& lightPosition, const glm::vec3& lightColor, const Ray& ray, const HitInfo& hitInfo)
{
    glm::vec3 color = lightColor;
    glm::vec3 intersection = ray.origin + ray.t * ray.direction;
    Ray lightRay { lightPosition, intersection - lightPosition };
    HitInfo hInfo {};
    glm::vec3 position = lightPosition;

    int iterations = 0;
    int maxIterations = 50;

    while (iterations < maxIterations) {
        state.bvh.intersect(state, lightRay, hInfo);
        drawRay(lightRay, { 0, 1, 0 });
        if (lightRay.t == std::numeric_limits<float>::max())
            return glm::vec3(0.0f);
        position += (lightRay.t + 1e-6f) * lightRay.direction;
        if (glm::length(position - intersection) < 1e-2)
            break;
        color *= hInfo.material.kd * (1 - hInfo.material.transparency);
        lightRay = { position, intersection - position };
        iterations++;
    }

    return color;
}

// Given a single point light, compute its contribution towards an incident ray at an intersection point.
//
// Hint: you should use visibilityOfLightSample() to account for shadows, and if the light is visible, use
//       the result of computeShading(), whose submethods you should probably implement first in shading.cpp.
//
// - state;   the active scene, feature config, bvh, and a thread-safe sampler
// - light;   the PointLight object, see common.h
// - ray;     the incident ray to the current intersection
// - hitInfo; information about the current intersection
// - return;  reflected light along the incident ray, based on computeShading()
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 computeContributionPointLight(RenderState& state, const PointLight& light, const Ray& ray, const HitInfo& hitInfo)
{
    glm::vec3 p = ray.origin + ray.t * ray.direction;
    glm::vec3 l = glm::normalize(light.position - p);
    glm::vec3 v = -ray.direction;

    if (visibilityOfLightSample(state, light.position, light.color, ray, hitInfo) != glm::vec3(0.0f))
        return computeShading(state, v, l, visibilityOfLightSample(state, light.position, light.color, ray, hitInfo), hitInfo);

    return glm::vec3(0.0f);
}


// Given a single segment light, compute its contribution towards an incident ray at an intersection point
// by integrating over the segment, taking numSamples samples from the light source.
//
// Hint: you can sample the light by using sampleSegmentLight(state.sampler.next_1d(), ...);, which
//       you should implement first.
// Hint: you should use visibilityOfLightSample() to account for shadows, and if the sample is visible, use
//       the result of computeShading(), whose submethods you should probably implement first in shading.cpp.
//
// - state;      the active scene, feature config, bvh, and a thread-safe sampler
// - light;      the SegmentLight object, see common.h
// - ray;        the incident ray to the current intersection
// - hitInfo;    information about the current intersection
// - numSamples; the number of samples you need to take
// - return;     accumulated light along the incident ray, based on computeShading()
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 computeContributionSegmentLight(RenderState& state, const SegmentLight& light, const Ray& ray, const HitInfo& hitInfo, uint32_t numSamples)
{
    glm::vec3 color { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < numSamples; i++) {
        glm::vec3 lightColor, lightPosition;
        sampleSegmentLight(state.sampler.next_1d(), light, lightPosition, lightColor);
        if (visibilityOfLightSample(state, lightPosition, lightColor, ray, hitInfo) != glm::vec3(0.0f)) {
            glm::vec3 shading = computeShading(state, -ray.direction, glm::normalize(lightPosition - (ray.origin + ray.t * ray.direction)), visibilityOfLightSample(state, lightPosition, lightColor, ray, hitInfo), hitInfo);
            color = color + shading;
        }
    }

    color[0] = color[0] / numSamples;
    color[1] = color[1] / numSamples;
    color[2] = color[2] / numSamples;

    return color;

}


// Given a single parralelogram light, compute its contribution towards an incident ray at an intersection point
// by integrating over the parralelogram, taking numSamples samples from the light source, and applying
// shading.
//
// Hint: you can sample the light by using sampleParallelogramLight(state.sampler.next_1d(), ...);, which
//       you should implement first.
// Hint: you should use visibilityOfLightSample() to account for shadows, and if the sample is visible, use
//       the result of computeShading(), whose submethods you should probably implement first in shading.cpp.
//
// - state;      the active scene, feature config, bvh, and a thread-safe sampler
// - light;      the ParallelogramLight object, see common.h
// - ray;        the incident ray to the current intersection
// - hitInfo;    information about the current intersection
// - numSamples; the number of samples you need to take
// - return;     accumulated light along the incident ray, based on computeShading()
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 computeContributionParallelogramLight(RenderState& state, const ParallelogramLight& light, const Ray& ray, const HitInfo& hitInfo, uint32_t numSamples)
{
    glm::vec3 color { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < numSamples; i++) {
        glm::vec3 lightPosition, lightColor;

        // visual debug; switch the enableDebugDraw flag to see the difference in the sample distribution
        if (state.features.enableDebugDraw)
            sampleParallelogramLight(state.sampler.next_2d(), light, lightPosition, lightColor);
        else {
            float sample1 = state.sampler.next_1d();
            float sample2 = state.sampler.next_1d();
            sampleParallelogramLight(glm::vec2(sample2, sample1), light, lightPosition, lightColor);
        }

        glm::vec3 intersection = ray.origin + ray.t * ray.direction;
        Ray colorRay = { intersection, lightPosition - intersection, 1 };

        bool isNotBlocked = visibilityOfLightSample(state, lightPosition, lightColor, ray, hitInfo) != glm::vec3(0.0f);
        if (isNotBlocked) {
            glm::vec3 shading = computeShading(state, -ray.direction, glm::normalize(lightPosition - (ray.origin + ray.t * ray.direction)), visibilityOfLightSample(state, lightPosition, lightColor, ray, hitInfo), hitInfo);
            color = color + shading;
        }

        HitInfo hInfo {};
        state.bvh.intersect(state, colorRay, hInfo);

        // visual debug
        if (state.features.enableDebugDraw) {
            if (isNotBlocked)
                drawRay(colorRay, lightColor);
            else
                drawRay(colorRay, { 1, 0, 0 });
        }
    }

    color[0] = color[0] / numSamples;
    color[1] = color[1] / numSamples;
    color[2] = color[2] / numSamples;

    return color;
}

// This function is provided as-is. You do not have to implement it.
// Given a sampled position on some light, and the emitted color at this position, return the actual
// light that is visible from the provided ray/intersection, or 0 if this is not the case.
// This forwards to visibilityOfLightSampleBinary/visibilityOfLightSampleTransparency based on settings.
//
// - state;         the active scene, feature config, and the bvh
// - lightPosition; the sampled position on some light source
// - lightColor;    the sampled color emitted at lightPosition
// - ray;           the incident ray to the current intersection
// - hitInfo;       information about the current intersection
// - return;        the visible light color that reaches the intersection
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 visibilityOfLightSample(RenderState& state, const glm::vec3& lightPosition, const glm::vec3& lightColor, const Ray& ray, const HitInfo& hitInfo)
{
    if (!state.features.enableShadows) {
        // Shadows are disabled in the renderer
        return lightColor;
    } else if (!state.features.enableTransparency) {
        // Shadows are enabled but transparency is disabled
        return visibilityOfLightSampleBinary(state, lightPosition, lightColor, ray, hitInfo) ? lightColor : glm::vec3(0);
    } else {
        // Shadows and transparency are enabled
        return visibilityOfLightSampleTransparency(state, lightPosition, lightColor, ray, hitInfo);
    }
}

// This function is provided as-is. You do not have to implement it.
glm::vec3 computeLightContribution(RenderState& state, const Ray& ray, const HitInfo& hitInfo)
{
    // Iterate over all lights
    glm::vec3 Lo { 0.0f };
    for (const auto& light : state.scene.lights) {
        if (std::holds_alternative<PointLight>(light)) {
            Lo += computeContributionPointLight(state, std::get<PointLight>(light), ray, hitInfo);
        } else if (std::holds_alternative<SegmentLight>(light)) {
            Lo += computeContributionSegmentLight(state, std::get<SegmentLight>(light), ray, hitInfo, state.features.numShadowSamples);
        } else if (std::holds_alternative<ParallelogramLight>(light)) {
            Lo += computeContributionParallelogramLight(state, std::get<ParallelogramLight>(light), ray, hitInfo, state.features.numShadowSamples);
        }
    }
    return Lo;
}