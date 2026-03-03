#include "draw.h"
#include <framework/opengl_includes.h>
// Suppress warnings in third-party code.
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#ifdef __APPLE__
#include <OpenGL/GLU.h>
#else
#ifdef WIN32
// Windows.h includes a ton of stuff we don't need, this macro tells it to include less junk.
#define WIN32_LEAN_AND_MEAN
// Disable legacy macro of min/max which breaks completely valid C++ code (std::min/std::max won't work).
#define NOMINMAX
// GLU requires Windows.h on Windows :-(.
#include <Windows.h>
#endif
#include <GL/glu.h>
#endif
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
DISABLE_WARNINGS_POP()
#include <algorithm>
#include "extra.cpp"

static void setMaterial(const Material& material)
{
    // Set the material color of the shape.
    const glm::vec4 kd4 { material.kd, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, glm::value_ptr(kd4));

    const glm::vec4 zero { 0.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, glm::value_ptr(zero));
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, glm::value_ptr(zero));
}

void drawExampleOfCustomVisualDebug()
{
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 1.0f, 0.0f);
    glEnd();
}


void drawTriangle (const Vertex& v0, const Vertex& v1, const Vertex& v2 ) {
    glBegin(GL_TRIANGLES);
        glNormal3fv(glm::value_ptr(v0.normal));
        glVertex3fv(glm::value_ptr(v0.position));
        glNormal3fv(glm::value_ptr(v1.normal));
        glVertex3fv(glm::value_ptr(v1.position));
        glNormal3fv(glm::value_ptr(v2.normal));
        glVertex3fv(glm::value_ptr(v2.position));
    glEnd();
}

void drawMesh(const Mesh& mesh)
{
    setMaterial(mesh.material);

    glBegin(GL_TRIANGLES);
    for (const auto& triangleIndex : mesh.triangles) {
        for (int i = 0; i < 3; i++) {
            const auto& vertex = mesh.vertices[triangleIndex[i]];
            glNormal3fv(glm::value_ptr(vertex.normal));
            glVertex3fv(glm::value_ptr(vertex.position));
        }
    }
    glEnd();
}

static void drawSphereInternal(const glm::vec3& center, float radius)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    const glm::mat4 transform = glm::translate(glm::identity<glm::mat4>(), center);
    glMultMatrixf(glm::value_ptr(transform));
    auto quadric = gluNewQuadric();
    gluSphere(quadric, radius, 40, 20);
    gluDeleteQuadric(quadric);
    glPopMatrix();
}

void drawSphere(const Sphere& sphere)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    setMaterial(sphere.material);
    drawSphereInternal(sphere.center, sphere.radius);
    glPopAttrib();
}

void drawSphere(const glm::vec3& center, float radius, const glm::vec3& color /*= glm::vec3(1.0f)*/)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glColor4f(color.r, color.g, color.b, 1.0f);
    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_FILL);
    drawSphereInternal(center, radius);
    glPopAttrib();
}

static void drawAABBInternal(const AxisAlignedBox& box)
{
    glPushMatrix();

    // front      back
    // 3 ----- 2  7 ----- 6
    // |       |  |       |
    // |       |  |       |
    // 0 ------1  4 ------5

    glBegin(GL_QUADS);
    glNormal3f(0, 0, -1);
    glVertex3f(box.lower.x, box.upper.y, box.lower.z); //3
    glVertex3f(box.upper.x, box.upper.y, box.lower.z); //2
    glVertex3f(box.upper.x, box.lower.y, box.lower.z); //1
    glVertex3f(box.lower.x, box.lower.y, box.lower.z); //0

    glNormal3f(0, 0, 1);
    glVertex3f(box.upper.x, box.lower.y, box.upper.z); //5
    glVertex3f(box.upper.x, box.upper.y, box.upper.z); //6
    glVertex3f(box.lower.x, box.upper.y, box.upper.z); //7
    glVertex3f(box.lower.x, box.lower.y, box.upper.z); //4

    glNormal3f(1, 0, 0);
    glVertex3f(box.upper.x, box.upper.y, box.lower.z); //2
    glVertex3f(box.upper.x, box.upper.y, box.upper.z); //6
    glVertex3f(box.upper.x, box.lower.y, box.upper.z); //5
    glVertex3f(box.upper.x, box.lower.y, box.lower.z); //1

    glNormal3f(-1, 0, 0);
    glVertex3f(box.lower.x, box.lower.y, box.upper.z); //4
    glVertex3f(box.lower.x, box.upper.y, box.upper.z); //7
    glVertex3f(box.lower.x, box.upper.y, box.lower.z); //3
    glVertex3f(box.lower.x, box.lower.y, box.lower.z); //0

    glNormal3f(0, 1, 0);
    glVertex3f(box.lower.x, box.upper.y, box.upper.z); //7
    glVertex3f(box.upper.x, box.upper.y, box.upper.z); //6
    glVertex3f(box.upper.x, box.upper.y, box.lower.z); //2
    glVertex3f(box.lower.x, box.upper.y, box.lower.z); //3

    glNormal3f(0, -1, 0);
    glVertex3f(box.upper.x, box.lower.y, box.lower.z); //1
    glVertex3f(box.upper.x, box.lower.y, box.upper.z); //5
    glVertex3f(box.lower.x, box.lower.y, box.upper.z); //4
    glVertex3f(box.lower.x, box.lower.y, box.lower.z); //0
    glEnd();

    glPopMatrix();
}

void drawAABB(const AxisAlignedBox& box, DrawMode drawMode, const glm::vec3& color, float transparency)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glColor4f(color.r, color.g, color.b, transparency);
    if (drawMode == DrawMode::Filled) {
        glPolygonMode(GL_FRONT, GL_FILL);
        glPolygonMode(GL_BACK, GL_FILL);
    } else {
        glPolygonMode(GL_FRONT, GL_LINE);
        glPolygonMode(GL_BACK, GL_LINE);
    }
    drawAABBInternal(box);
    glPopAttrib();
}

void drawScene(const Scene& scene)
{
    for (const auto& mesh : scene.meshes)
        drawMesh(mesh);
    for (const auto& sphere : scene.spheres)
        drawSphere(sphere);
}

void drawRay(const Ray& ray, const glm::vec3& color)
{
    const glm::vec3 hitPoint = ray.origin + std::clamp(ray.t, 0.0f, 100.0f) * ray.direction;
    const bool hit = (ray.t < std::numeric_limits<float>::max());

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);

    glColor3fv(glm::value_ptr(color));
    glVertex3fv(glm::value_ptr(ray.origin));
    glColor3fv(glm::value_ptr(color));
    glVertex3fv(glm::value_ptr(hitPoint));
    glEnd();

    if (hit)
        drawSphere(hitPoint, 0.005f, color);

    glPopAttrib();
}
void drawTextureForm(const HitInfo& hit_info, const Scene& scene)
{
    if (hit_info.material.kdTexture == NULL)
        return;
    const auto texture = *hit_info.material.kdTexture;
    for (int i = 0; i < texture.width; ++i) {
        for (int j = 0; j < texture.height; ++j) {
            glBegin(GL_QUADS);
            glVertex3f(static_cast<float>(i) / texture.width, static_cast<float>(j) / texture.height, 1.0f);
            glVertex3f(static_cast<float>(i + 1) / texture.width, static_cast<float>(j) / texture.height, 1.0f);
            glVertex3f(static_cast<float>(i + 1) / texture.width, static_cast<float>(j + 1) / texture.height, 1.0f);
            glVertex3f(static_cast<float>(i) / texture.width, static_cast<float>(j + 1) / texture.height, 1.0f);
            const auto& pixel = texture.get_pixel(j * texture.height + i);
            glColor3f(pixel.r, pixel.g, pixel.b);
            glEnd();
        }
    }
    for (const auto mesh : scene.meshes) {
        for (const auto indices : mesh.triangles) {
            const Vertex a = mesh.vertices[indices[0]];
            const Vertex b = mesh.vertices[indices[1]];
            const Vertex c = mesh.vertices[indices[2]];
            glBegin(GL_LINE_LOOP);
            glVertex3f(a.texCoord.x, a.texCoord.y, 0.996f);
            glVertex3f(b.texCoord.x, b.texCoord.y, 0.996f);
            glVertex3f(c.texCoord.x, c.texCoord.y, 0.996f);
            glColor3f(0.1, 0.3, 0.6);
            glEnd();
        }
    }
    drawSphere({ hit_info.texCoord.x, hit_info.texCoord.y, 0.992f }, 0.02, { 1, 0, 0 });
}
void drawSquare(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
    glBegin(GL_QUADS);
    glColor3f(1, 1, 1);
    glVertex3f(a.x, a.y, a.z);
    glVertex3f(b.x, b.y, b.z);
    glVertex3f(c.x, c.y, c.z);
    glVertex3f(d.x, d.y, d.z);
    glEnd();
}
void drawLine(const glm::vec3& a, const glm::vec3& b) {
    glBegin(GL_LINES);
    glColor3f(0.7, 0.6, 0);
    glVertex3f(a.x, a.y, a.z);
    glVertex3f(b.x, b.y, b.z);
    glEnd();
}
void drawSpline(const std::vector<glm::vec3>& controlPoints, glm::vec3 pos)
{
    renderSpline(controlPoints, pos);
}

void drawSceneAtTime(const Scene& scene, const Features& features, const Trackball& camera, Screen& screen, float time) {
    renderSceneAtTime(scene, features, camera, screen, time);
}
