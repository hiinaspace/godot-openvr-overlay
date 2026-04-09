#pragma once

#include <godot_cpp/variant/transform3d.hpp>
#include <openvr.h>

namespace ovr_math {

struct FrustumApproximation {
    float size = 0.0f;
    godot::Vector2 offset;
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
};

// Convert row-major HmdMatrix34_t to Godot Transform3D.
// Both OpenVR and Godot use right-handed +Y up -Z forward, so no axis flip needed.
inline godot::Transform3D to_godot(const vr::HmdMatrix34_t &m) {
    return godot::Transform3D(
        godot::Basis(
            godot::Vector3(m.m[0][0], m.m[1][0], m.m[2][0]),
            godot::Vector3(m.m[0][1], m.m[1][1], m.m[2][1]),
            godot::Vector3(m.m[0][2], m.m[1][2], m.m[2][2])),
        godot::Vector3(m.m[0][3], m.m[1][3], m.m[2][3]));
}

// Multiply two HmdMatrix34_t as rigid transforms (rotation + translation).
inline vr::HmdMatrix34_t multiply(const vr::HmdMatrix34_t &a, const vr::HmdMatrix34_t &b) {
    vr::HmdMatrix34_t result{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.m[row][col] = 0;
            for (int k = 0; k < 3; ++k)
                result.m[row][col] += a.m[row][k] * b.m[k][col];
        }
        result.m[row][3] += a.m[row][3];
    }
    return result;
}

// Invert a rigid-body HmdMatrix34_t (transpose rotation, negate translated origin).
inline vr::HmdMatrix34_t invert(const vr::HmdMatrix34_t &m) {
    vr::HmdMatrix34_t result{};
    // Transpose rotation part
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            result.m[r][c] = m.m[c][r];
    // Translation: -R^T * t
    for (int r = 0; r < 3; ++r) {
        result.m[r][3] = 0;
        for (int k = 0; k < 3; ++k)
            result.m[r][3] -= result.m[r][k] * m.m[k][3];
    }
    return result;
}

// Approximate an arbitrary frustum with Camera3D::set_frustum(size, offset, near, far),
// whose width/height ratio is locked to the viewport aspect.
inline FrustumApproximation approximate_camera_frustum(float left, float right,
                                                       float bottom, float top,
                                                       float viewport_aspect) {
    FrustumApproximation result;
    result.size = top - bottom;
    result.offset = godot::Vector2((left + right) * 0.5f, (bottom + top) * 0.5f);

    const float half_width = result.size * viewport_aspect * 0.5f;
    const float half_height = result.size * 0.5f;

    result.left = result.offset.x - half_width;
    result.right = result.offset.x + half_width;
    result.bottom = result.offset.y - half_height;
    result.top = result.offset.y + half_height;
    return result;
}

} // namespace ovr_math
