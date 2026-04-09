#pragma once

#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/projection.hpp>
#include <openvr.h>

namespace ovr_math {

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

// Build an asymmetric OpenGL-style projection matrix from raw tangent frustum values.
// frustum_left/right/top/bottom are the tangents from GetProjectionRaw().
//
// NOTE: OpenVR GetProjectionRaw uses screen-Y-down convention:
//   top is negative (upper edge of frustum = -Y direction)
//   bottom is positive (lower edge of frustum = +Y direction)
// Godot's create_frustum uses +Y-up convention: bottom < 0, top > 0.
// So we swap top/bottom when passing to Godot.
inline godot::Projection make_projection(float frustum_left, float frustum_right,
                                         float frustum_top, float frustum_bottom,
                                         float near_z, float far_z) {
    float l = near_z * frustum_left;
    float r = near_z * frustum_right;
    // OpenVR top (negative) becomes Godot bottom; OpenVR bottom (positive) becomes Godot top
    float godot_bottom = near_z * frustum_top;
    float godot_top    = near_z * frustum_bottom;
    return godot::Projection::create_frustum(l, r, godot_bottom, godot_top, near_z, far_z);
}

} // namespace ovr_math
