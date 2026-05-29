#pragma once
#include <cmath>

namespace myglm
{

    const float PI = 3.14159265358979323846f;

    inline float radians(float degrees)
    {
        return degrees * (PI / 180.0f);
    }

    struct vec3
    {
        float x, y, z;

        vec3() : x(0.0f), y(0.0f), z(0.0f) {}
        vec3(float v) : x(v), y(v), z(v) {}
        vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

        vec3 &operator+=(const vec3 &v)
        {
            x += v.x;
            y += v.y;
            z += v.z;
            return *this;
        }
        vec3 &operator-=(const vec3 &v)
        {
            x -= v.x;
            y -= v.y;
            z -= v.z;
            return *this;
        }
        vec3 &operator*=(const vec3 &v)
        {
            x *= v.x;
            y *= v.y;
            z *= v.z;
            return *this;
        }
        vec3 &operator/=(const vec3 &v)
        {
            x /= v.x;
            y /= v.y;
            z /= v.z;
            return *this;
        }

        vec3 &operator*=(float s)
        {
            x *= s;
            y *= s;
            z *= s;
            return *this;
        }
        vec3 &operator/=(float s)
        {
            x /= s;
            y /= s;
            z /= s;
            return *this;
        }
    };

    // Multiplies a vec3 by a float
    inline vec3 operator*(const vec3 &v, float s)
    {
        return vec3(v.x * s, v.y * s, v.z * s);
    }

    // Multiplies a float by a vec3
    inline vec3 operator*(float s, const vec3 &v)
    {
        return vec3(v.x * s, v.y * s, v.z * s);
    }

    // Division by a float
    inline vec3 operator/(const vec3 &v, float s)
    {
        return vec3(v.x / s, v.y / s, v.z / s);
    }
    // ejcl until normalize
    inline vec3 operator+(const vec3 &a, const vec3 &b)
    {
        return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
    inline vec3 operator-(const vec3 &a, const vec3 &b)
    {
        return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
    }
    inline float dot(const vec3 &a, const vec3 &b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    inline vec3 cross(const vec3 &a, const vec3 &b)
    {
        return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
    }
    inline float length(const vec3 &v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
    inline vec3 normalize(const vec3 &v)
    {
        float len = length(v);

        if (len == 0.0f)
            return vec3(0.0f);

        return vec3(
            v.x / len,
            v.y / len,
            v.z / len);
    }

    struct vec4
    {
        float x, y, z, w;
        vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
        vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    };

    struct mat4
    {
        float m[4][4];
        mat4()
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    m[i][j] = 0.0f;
        }

        mat4(float diagonal)
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    m[i][j] = (i == j) ? diagonal : 0.0f;
        }

        mat4 operator*(const mat4 &other) const
        {
            mat4 result(0.0f);
            for (int col = 0; col < 4; ++col)
            {
                for (int row = 0; row < 4; ++row)
                {
                    result.m[col][row] =
                        m[0][row] * other.m[col][0] +
                        m[1][row] * other.m[col][1] +
                        m[2][row] * other.m[col][2] +
                        m[3][row] * other.m[col][3];
                }
            }
            return result;
        }
    };

    inline vec4 operator*(const mat4 &m, const vec4 &v)
{
    vec4 r;

    r.x =
        m.m[0][0] * v.x +
        m.m[1][0] * v.y +
        m.m[2][0] * v.z +
        m.m[3][0] * v.w;

    r.y =
        m.m[0][1] * v.x +
        m.m[1][1] * v.y +
        m.m[2][1] * v.z +
        m.m[3][1] * v.w;

    r.z =
        m.m[0][2] * v.x +
        m.m[1][2] * v.y +
        m.m[2][2] * v.z +
        m.m[3][2] * v.w;

    r.w =
        m.m[0][3] * v.x +
        m.m[1][3] * v.y +
        m.m[2][3] * v.z +
        m.m[3][3] * v.w;

    return r;
}
    inline const float *value_ptr(const vec4 &v) { return &v.x; }
    inline const float *value_ptr(const mat4 &m) { return &m.m[0][0]; }

    inline mat4 translate(const mat4 &m, const vec3 &v)
    {
        mat4 result = m;
        result.m[3][0] = m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0];
        result.m[3][1] = m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1];
        result.m[3][2] = m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2];
        result.m[3][3] = m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3];
        return result;
    }

    inline mat4 scale(const mat4 &m, const vec3 &v)
    {
        mat4 result;
        result.m[0][0] = m.m[0][0] * v.x;
        result.m[0][1] = m.m[0][1] * v.x;
        result.m[0][2] = m.m[0][2] * v.x;
        result.m[0][3] = m.m[0][3] * v.x;
        result.m[1][0] = m.m[1][0] * v.y;
        result.m[1][1] = m.m[1][1] * v.y;
        result.m[1][2] = m.m[1][2] * v.y;
        result.m[1][3] = m.m[1][3] * v.y;
        result.m[2][0] = m.m[2][0] * v.z;
        result.m[2][1] = m.m[2][1] * v.z;
        result.m[2][2] = m.m[2][2] * v.z;
        result.m[2][3] = m.m[2][3] * v.z;
        result.m[3][0] = m.m[3][0];
        result.m[3][1] = m.m[3][1];
        result.m[3][2] = m.m[3][2];
        result.m[3][3] = m.m[3][3];
        return result;
    }

    inline mat4 rotate(const mat4 &m, float angle, const vec3 &v)
    {
        float c = std::cos(angle);
        float s = std::sin(angle);
        float t = 1.0f - c;

        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        float nx = v.x / len;
        float ny = v.y / len;
        float nz = v.z / len;

        mat4 rot(1.0f);
        rot.m[0][0] = c + nx * nx * t;
        rot.m[0][1] = ny * nx * t + nz * s;
        rot.m[0][2] = nz * nx * t - ny * s;

        rot.m[1][0] = nx * ny * t - nz * s;
        rot.m[1][1] = c + ny * ny * t;
        rot.m[1][2] = nz * ny * t + nx * s;

        rot.m[2][0] = nx * nz * t + ny * s;
        rot.m[2][1] = ny * nz * t - nx * s;
        rot.m[2][2] = c + nz * nz * t;

        return m * rot;
    }

    inline mat4 perspective(float fovy, float aspect, float zNear, float zFar)
    {
        // Assert to prevent division by zero in case of invalid inputs
        // assert(abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

        // Since your mat4() constructor initializes all values to 0.0f,
        // we only need to set the values that are non-zero.
        mat4 result;

        float tanHalfFovy = std::tan(fovy / 2.0f);

        // Column 0
        result.m[0][0] = 1.0f / (aspect * tanHalfFovy);

        // Column 1
        result.m[1][1] = 1.0f / tanHalfFovy;

        // Column 2
        result.m[2][2] = -(zFar + zNear) / (zFar - zNear);
        result.m[2][3] = -1.0f; // This is what flips the Z axis and prepares for perspective division

        // Column 3
        result.m[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);

        return result;
    }
    // ejcl
    inline mat4 lookAt(const vec3 &eye, const vec3 &center, const vec3 &up)
    {
        vec3 f = normalize(center - eye);
        vec3 s = normalize(cross(f, up));
        vec3 u = cross(s, f);

        mat4 result(1.0f);

        result.m[0][0] = s.x;
        result.m[1][0] = s.y;
        result.m[2][0] = s.z;

        result.m[0][1] = u.x;
        result.m[1][1] = u.y;
        result.m[2][1] = u.z;

        result.m[0][2] = -f.x;
        result.m[1][2] = -f.y;
        result.m[2][2] = -f.z;

        result.m[3][0] = -dot(s, eye);
        result.m[3][1] = -dot(u, eye);
        result.m[3][2] = dot(f, eye);

        return result;
    }
}