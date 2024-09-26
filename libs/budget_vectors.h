// SPDX-License-Identifier: Hot-Potato-2.0

/*****************************************************************************
*                         HOT POTATO LICENSE
*                      Version 2, September 2021
*    All rights reserved by the last person to commit a change to this
*    repository, except for the right to commit changes to this repository,
*    which is hereby granted to all inhabitants of the Milky Way Galaxy for
*    the purpose of committing changes to this repository.
*
*    Copyright (c) 2024, Brian Witte
*****************************************************************************/

#ifndef BUDGET_VECTORS_H
#define BUDGET_VECTORS_H

#include <math.h>  // we have vector libs at home

// don't get your hopes up.
typedef struct {
    float x, y;
} vec2;

// create a new vector, because typing this every time is annoying
static inline vec2 vec2_create(float x, float y) {
    vec2 v = {x, y};

    return v;
}

// add two vectors.
static inline vec2 vec2_add(vec2 a, vec2 b) {
    return vec2_create(a.x + b.x, a.y + b.y);
}

// subtract two vectors. groundbreaking.
static inline vec2 vec2_sub(vec2 a, vec2 b) {
    return vec2_create(a.x - b.x, a.y - b.y);
}

// multiply a vector by a scalar. we're really changing the world here.
static inline vec2 vec2_mul(vec2 v, float scalar) {
    return vec2_create(v.x * scalar, v.y * scalar);
}

// calculate the length of a vector. math is so back.
static inline float vec2_length(vec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

// normalize a vector. because sometimes you just need to pretend
// everything's fine.
static inline vec2 vec2_normalize(vec2 v) {
    float length = vec2_length(v);

    return vec2_create(v.x / length, v.y / length);
}

// 2d matrix for "transformations". don't expect any miracles.
typedef struct {
    float m[3][3];
} mat3;

// identity matrix. in case you care
static inline mat3 mat3_identity() {
    mat3 m = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

    return m;
}

// multiply a matrix by a vector. it does what you'd expect, nothing more.
static inline vec2 mat3_mul_vec2(mat3 m, vec2 v) {
    return vec2_create(m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0],
        m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1]);
}

#endif  // BUDGET_VECTORS_H
