#pragma once

#include "space_typers_platform.h"

#define _USE_MATH_DEFINES //M_PI
#include <math.h> //sinf

#include "space_typers_img.h"

#if _DEBUG //TODO(fran): change to my own flag and set it with each compiler's flags, or even simpler just let the user set the flag
#define game_assert(expression) if(!(expression)){*(int*)0=0;}
#else
#define game_assert(expression) 
#endif

#define sizeof_arr(arr) sizeof(arr)/sizeof((arr)[0])

f32 minimum(f32 a, f32 b) { //TODO(fran): create math .h file or similar
    return a < b ? a : b;
}
f32 maximum(f32 a, f32 b) { 
    return a > b ? a : b;
}

struct argb_f32 { f32 a, r, g, b; };


struct img {
    int width, height, channels, bytes_per_channel;
    //v2_i32 align; //TODO(fran): add align offset to change the position of the bitmap relative to a point (Handmade hero day 39)
    void* mem;
};

//struct Word {
//    v2_f32 pos;
//    //v2_f32 speed;
//    utf16 txt[25];
//};

//struct game_world {
//    int width, height;
//};

struct rc {
    v2_f32 center;
    v2_f32 radius;

    v2_f32 get_min() {
        v2_f32 res = center - radius;
        return res;
    }

    v2_f32 get_max() {
        v2_f32 res = center + radius;
        return res;
    }
};

rc rc_min_max(v2_f32 min, v2_f32 max) { //NOTE REMEMBER: I like this idea
    rc res;
    res.center = (max + min) / 2.f;
    res.radius = (max - min) / 2.f;
    return res;
}

rc rc_center_radius(v2_f32 center, v2_f32 radius) {
    rc res;
    res.center = center;
    res.radius = radius;
    return res;
}

bool is_in_rc(v2_f32 p, rc r) {
    //NOTE: we dont include the final coordinate point
    bool res = p.x >= r.get_min().x && p.x < r.get_max().x && p.y >= r.get_min().y && p.y < r.get_max().y;
    return res;
}

struct colored_rc {
    rc rect;
    argb_f32 color;
    v2_f32 velocity;
};

//struct game_state_TODO {
//    float accumulated_time;
//    std::vector<Word> words;
//    float dt;
//    std::wstring current_word;
//    float word_speed;
//    float next_word_time;
//    game_world world;//TODO(fran): real gameworld and camera to determinate what to show
//};

struct game_level_map {
    //v2_f32 origin;
    //f32 unit;
    //colored_rc wall; //TODO(fran): this will be a pointer to n walls, we should store a counter too
    colored_rc* walls;
    u32 wall_count;
    //colored_rc word;
    colored_rc* words;
    u32 word_count;
};

struct game_stage {
    game_level_map* lvls;
    u32 current_lvl;
    u32 level_count;
};

struct game_memory_arena {
    u32 sz;
    u8* base;
    u32 used;
};

void initialize_arena(game_memory_arena* arena, u8* mem, u32 sz) {
    arena->sz = sz;
    arena->base = mem;
    arena->used = 0;
}

void* _push_mem(game_memory_arena* arena, u32 sz) {
    game_assert((arena->used + sz) <= arena->sz);//TODO(fran): < or <= ?
    void* res = arena->base + arena->used;
    arena->used += sz;
    return res;
} //TODO(fran): set mem to zero? (not really necessary since we request the mem to be cleared to zero)

#define push_mem(arena,type) (type*)_push_mem(arena,sizeof(type))

#define push_arr(arena,type,sz) (type*)_push_mem(arena,sizeof(type)*sz)

struct game_world{
    game_stage* stages;
    u32 stage_count;
    u32 current_stage;
};

struct game_entity {
    v2_f32 pos;
    v2_f32 radius;
    argb_f32 color;
    v2_f32 velocity;
    //v2_f32 acceleration; TODO
    bool collides;
    //utf16 txt[25];
    //TODO(fran): add enum for entity type?
};

struct game_state {
    //int xoff;
    //int yoff;
    //int hz;
    f32 word_height_meters;
    i32 word_height_pixels;
    f32 word_meters_to_pixels;
    v2_f32 lower_left_pixels;
    game_world world;//TODO(fran): for now there'll only be one world but we may add support for more later
    game_memory_arena memory_arena;

    img DEBUG_background;
    img DEBUG_menu;
    img DEBUG_mouse;

    v2_f32 camera;

    game_entity entities[20];
    u32 entity_count;
};

//TODO(fran): check what casey said about adding static to functions to reduce link/compilation time (explanation in day 53, min ~1:05:00)

