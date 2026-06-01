#pragma once
#include <cmath>

namespace myglm {

    const float PI = 3.14159265358979323846f;

    inline float radians(float degrees) {
        return degrees * (PI / 180.0f);
    }

    struct vec4; // Forward declaration so vec3 can use it

    struct vec3 {
        float x, y, z;
        
        vec3() : x(0.0f), y(0.0f), z(0.0f) {}
        vec3(float v) : x(v), y(v), z(v) {}
        vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
        vec3(const vec4& v); // Defined below once vec4 is known

        vec3& operator+=(const vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
        vec3& operator-=(const vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
        vec3& operator*=(const vec3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
        vec3& operator/=(const vec3& v) { x /= v.x; y /= v.y; z /= v.z; return *this; }
        
        vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
        vec3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
    };

    struct vec4 {
        float x, y, z, w;
        vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
        vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
        
        // NEW: Construct a vec4 from a vec3 and a float (needed for Camera)
        vec4(const vec3& v, float _w) : x(v.x), y(v.y), z(v.z), w(_w) {}
    };

    // NEW: Definition of vec3 constructor that takes a vec4
    inline vec3::vec3(const vec4& v) : x(v.x), y(v.y), z(v.z) {}

    // 1. Vector arithmetic operators
    inline vec3 operator+(const vec3& a, const vec3& b) {
        return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
    
    inline vec3 operator-(const vec3& a, const vec3& b) {
        return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    inline vec3 operator*(const vec3& v, float s) {
        return vec3(v.x * s, v.y * s, v.z * s);
    }

    inline vec3 operator*(float s, const vec3& v) {
        return vec3(v.x * s, v.y * s, v.z * s);
    }

    // 2. Essential 3D Math Functions for the Camera
    inline float dot(const vec3& a, const vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline vec3 cross(const vec3& a, const vec3& b) {
        return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    inline vec3 normalize(const vec3& v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > 0.000001f) {
            return vec3(v.x / len, v.y / len, v.z / len);
        }
        return vec3(0.0f, 0.0f, 0.0f);
    }

    struct mat4 {
        float m[4][4];
        mat4() {
            for(int i = 0; i < 4; ++i)
                for(int j = 0; j < 4; ++j)
                    m[i][j] = 0.0f;
        }

        mat4(float diagonal) {
            for(int i = 0; i < 4; ++i)
                for(int j = 0; j < 4; ++j)
                    m[i][j] = (i == j) ? diagonal : 0.0f;
        }

        mat4 operator*(const mat4& other) const {
            mat4 result(0.0f); 
            for(int col = 0; col < 4; ++col) {
                for(int row = 0; row < 4; ++row) {
                    result.m[col][row] = 
                        m[0][row] * other.m[col][0] +
                        m[1][row] * other.m[col][1] +
                        m[2][row] * other.m[col][2] +
                        m[3][row] * other.m[col][3];
                }
            }
            return result;
        }
    } __attribute__((aligned(16))); // Optional alignment optimization

    // NEW: Matrix-Vector multiplication (needed for roll rotation matrix)
    inline vec4 operator*(const mat4& m, const vec4& v) {
        return vec4(
            m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0] * v.w,
            m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1] * v.w,
            m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2] * v.w,
            m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3] * v.w
        );
    }

    inline const float* value_ptr(const vec4& v) { return &v.x; }
    inline const float* value_ptr(const mat4& m) { return &m.m[0][0]; }

    inline mat4 translate(const mat4& m, const vec3& v) {
        mat4 result = m;
        result.m[3][0] = m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0];
        result.m[3][1] = m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1];
        result.m[3][2] = m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2];
        result.m[3][3] = m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3];
        return result;
    }

    inline mat4 scale(const mat4& m, const vec3& v) {
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

    inline mat4 rotate(const mat4& m, float angle, const vec3& v) {
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

    // NEW: The missing lookAt function
    inline mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) {
        vec3 f = normalize(vec3(center.x - eye.x, center.y - eye.y, center.z - eye.z));
        vec3 s = normalize(cross(f, up));
        vec3 u = cross(s, f);

        mat4 Result(1.0f);
        Result.m[0][0] = s.x;
        Result.m[1][0] = s.y;
        Result.m[2][0] = s.z;
        Result.m[0][1] = u.x;
        Result.m[1][1] = u.y;
        Result.m[2][1] = u.z;
        Result.m[0][2] = -f.x;
        Result.m[1][2] = -f.y;
        Result.m[2][2] = -f.z;
        Result.m[3][0] = -dot(s, eye);
        Result.m[3][1] = -dot(u, eye);
        Result.m[3][2] =  dot(f, eye);
        
        return Result;
    }
    
    inline mat4 perspective(float fovy, float aspect, float zNear, float zFar) {
        mat4 result;
        float tanHalfFovy = std::tan(fovy / 2.0f);
        
        result.m[0][0] = 1.0f / (aspect * tanHalfFovy);
        result.m[1][1] = 1.0f / tanHalfFovy;
        result.m[2][2] = -(zFar + zNear) / (zFar - zNear);
        result.m[2][3] = -1.0f;
        result.m[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);
        
        return result;
    }

}