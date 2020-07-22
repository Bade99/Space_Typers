#pragma once

//V2_F32
struct v2_f32 {
    union {
        struct {
        f32 x, y;
        };
        f32 comp[2];
    };

    v2_f32& operator+=(const v2_f32& rhs);

    v2_f32& operator-=(const v2_f32& rhs);

    v2_f32& operator*=(f32 rhs_scalar);
};

v2_f32 operator*(v2_f32 v, f32 scalar) {
    v2_f32 res;
    res.x = v.x * scalar;
    res.y = v.y * scalar;
    return res;
}

v2_f32 operator*(f32 scalar, v2_f32 v) { //NOTE: waiting for C++20 where I think they let you define the operation only one way
    v2_f32 res = v*scalar;
    return res;
}

v2_f32 operator-(v2_f32 v1, v2_f32 v2) {
    v2_f32 res;
    res.x = v1.x - v2.x;
    res.y = v1.y - v2.y;
    return res;
}

v2_f32 operator+(v2_f32 v1, v2_f32 v2) {
    v2_f32 res;
    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    return res;
}

v2_f32 operator-(v2_f32 v) {
    v2_f32 res;
    res.x = -v.x;
    res.y = -v.y;
    return res;
}

v2_f32& v2_f32::operator-=(const v2_f32& rhs) {
    *this = *this - rhs; //NOTE: remember to declare + - * ... before += -=, otherwise it will not find the functions
    return *this;
}

v2_f32& v2_f32::operator+=(const v2_f32& rhs) { //TODO(fran): is it necessary to use const& instead of a simple copy?
    *this = *this + rhs;
    return *this;
}

v2_f32& v2_f32::operator*=(f32 rhs_scalar) {
    *this = *this * rhs_scalar;
    return *this;
}

f32 dot(v2_f32 v1, v2_f32 v2) {
    f32 res = v1.x * v2.x + v1.y * v2.y;
    return res;
}

f32 lenght_sq(v2_f32 v) {
    f32 res;
    res = dot(v, v);
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