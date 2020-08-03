#pragma once

//NOTE: by default the vectors are to represent f32 values, therefore we will TODO(fran): change v2_f32 to v2

//V2 (F32)
struct v2 {
    union {
        struct {
        f32 x, y;
        };
        f32 comp[2];
    };

    v2& operator+=(const v2& rhs);

    v2& operator-=(const v2& rhs);

    v2& operator*=(f32 rhs_scalar);
};

v2 operator*(v2 v, f32 scalar) {
    v2 res;
    res.x = v.x * scalar;
    res.y = v.y * scalar;
    return res;
}

v2 operator*(f32 scalar, v2 v) { //NOTE: waiting for C++20 where I think they let you define the operation only one way
    v2 res = v*scalar;
    return res;
}

v2 operator/(v2 v, f32 scalar) {
    v2 res;
    res.x = v.x / scalar;
    res.y = v.y / scalar;
    return res;
}

v2 operator-(v2 a, v2 b) {
    v2 res;
    res.x = a.x - b.x;
    res.y = a.y - b.y;
    return res;
}

v2 operator+(v2 a, v2 b) {
    v2 res;
    res.x = a.x + b.x;
    res.y = a.y + b.y;
    return res;
}

v2 operator-(v2 v) {
    v2 res;
    res.x = -v.x;
    res.y = -v.y;
    return res;
}

v2& v2::operator-=(const v2& rhs) {
    *this = *this - rhs; //NOTE: remember to declare + - * ... before += -=, otherwise it will not find the functions
    return *this;
}

v2& v2::operator+=(const v2& rhs) { //TODO(fran): is it necessary to use const& instead of a simple copy?
    *this = *this + rhs;
    return *this;
}

v2& v2::operator*=(f32 rhs_scalar) {
    *this = *this * rhs_scalar;
    return *this;
}

f32 dot(v2 a, v2 b) {
    f32 res = a.x * b.x + a.y * b.y;
    return res;
}

f32 lenght_sq(v2 v) {
    f32 res;
    res = dot(v, v);
    return res;
}

v2 v2_from_i32(i32 x, i32 y) {
    v2 res;
    res.x = (f32)x;
    res.y = (f32)y;
    return res;
}

//V2_I32
struct v2_i32 {
    i32 x, y;
};

//V3_I32
struct v3_i32 {
    i32 x, y, z;
};

//V4 (F32)
union v4 {
    struct {
        f32 x, y, z, w;
    };
    struct {
        f32 r,g,b,a;
    };
};

v4 operator*(v4 v, f32 scalar) {
    v4 res;
    res.x = v.x * scalar;
    res.y = v.y * scalar;
    res.z = v.z * scalar;
    res.w = v.w * scalar;
    return res;
}