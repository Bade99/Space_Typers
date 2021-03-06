#pragma once

#include "space_typers_platform.h"

#define ASSERTIONS

#ifdef ASSERTIONS //TODO(fran): change to my own flag and set it with each compiler's flags, or even simpler just let the user set the flag
#define game_assert(expression) if(!(expression)){*(int*)0=0;}
#else
#define game_assert(expression) 
#endif

#define _USE_MATH_DEFINES //M_PI
#include <math.h> //sinf

#include "space_typers_img.h"
#include "space_typers_font.h"

#include "space_typers_rectangle.h"

#include "space_typers_math.h"

//TODO(fran): create separate h file with arena things? so I dont have to put this in the middle of here
#define push_type(arena,type) (type*)_push_mem(arena,sizeof(type))

#define push_arr(arena,type,count) (type*)_push_mem(arena,sizeof(type)*count)

#define push_sz(arena,sz) _push_mem(arena,sz)

#define IMG_BYTES_PER_PIXEL 4 //NOTE: there's one byte for each channel
#define IMG_CHANNELS 4 //NOTE: all images are loaded and stored in rgba (maybe not that order)
struct img {
    i32 width;
    i32 height;
    i32 pitch;
    v2 alignment_percent; //NOTE: offset from the bottom left of the img that moves its position
    f32 width_over_height; //NOTE: in case you want to maintain the original aspect ratio, multiply any height by this to get the proper width
    void* mem;
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

enum game_entity_type {
    entity_null,
    entity_player,
    entity_word,
    entity_wall,
    entity_word_spawner
};

enum game_entity_flags {
    entity_flag_collides = (1 << 1), //does collision detection
    entity_flag_alive = (1 << 2), //NOTE: non alive entities are removed at the end of each frame
    entity_flag_solid = (1<<3), //acts as a collider for other objects
    //TODO(fran): add moveable flag?
};

struct game_entity_collision_area {
    v2 offset; //NOTE: offset from entity pos
    v2 radius;
};

struct game_entity_collision_area_group {
    game_entity_collision_area total_area;
    game_entity_collision_area* areas;
    u32 area_count;
};

struct game_entity {
    v2 pos;
    //v2 radius;
    game_entity_collision_area_group collision;
    v4 color;
    v2 velocity;
    v2 acceleration; //TODO: apply to every entity
    u32 flags;
    
    game_entity_type type;

    //wall
    f32 anim_wall_texture;

    //Word spawner
    f32 accumulated_time_sec;
    f32 time_till_next_word_sec;

    //v2_i32 align; //TODO(fran): add align offset to change the position of the bitmap relative to a point (Handmade hero day 39)

    utf32 txt[25];
    img image;

    u32 z_layer;
};

//NOTE REMEMBER: you cant check for multiple flags set at the same time, eg a|b|c will return true if any single one is set, not only if all are
bool is_set(const game_entity* e, u32 flag) {
    bool res = e->flags & flag;
    return res;
}

void add_flags(game_entity* e, u32 flags) {
    e->flags |= flags;
}

void clear_flags(game_entity* e, u32 flags) {
    e->flags &= ~flags;
}

//TODO(fran): layer_info structure that contains the layers of the level and properties that the renderer cares about, in this case scaling
//TODO(fran): add u32 layer to entities
struct layer {
    f32 scale_factor;
    f32 current_scale;
};
struct layer_info {
    layer layers[10];
    u32 layer_count;
};
//This allows us to, for example have the mouse in a layer with no scaling, have the background in a layer with little scaling
//and also sort draw order by layer, layer 0 being the least priority one

struct game_level_map {
    game_entity* entities;
    u32 entity_count;
    layer_info layer_nfo;
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
} //TODO(fran): set mem to zero?


struct game_world{
    game_stage* stages;
    u32 stage_count;
    u32 current_stage;
};

#define PERSPECTIVE 1

#include "space_typers_render_group.cpp"

struct pairwise_collision_rule {
    game_entity* a;
    game_entity* b; //TODO(fran): having straight pointers is probably a problem, maybe better is having indexes into the entity table or some global always incrementing index
    bool can_collide;

    pairwise_collision_rule* next_in_hash;
};

struct transient_state { //Anything allocated in the arena that contains this state can be destroyed at "any time" and reconstructed, but it'll live for as long as it can otherwise, aka does not get reset after each frame
    bool is_initialized = false;
    game_memory_arena transient_arena; //NOTE: transient does not necessarily mean it gets "destroyed" after each frame but that it can be thrown away at any time and the stuff inside regenerated

    i32 env_map_width;
    i32 env_map_height;
    environment_map TEST_top_env_map;
    environment_map TEST_bottom_env_map;
};

struct game_state {
    //int xoff;
    //int yoff;
    //int hz;
    //v2 lower_left_pixels;
    game_world world;//TODO(fran): for now there'll only be one world but we may add support for more later
    game_memory_arena permanent_arena;
    game_memory_arena one_frame_arena;//gets reset at the end of each frame

    img DEBUG_background;
    img DEBUG_menu;
    img DEBUG_mouse;

    img sphere_normal;
    img test_diffuse;

    img word_corner;
    img word_border;
    img word_inside;

    img wall_mask;
    img wall_tile;

    v2 camera;
    game_entity entities[20];
    u32 entity_count;

    pairwise_collision_rule* collision_rule_hash[256]; //NOTE: must be a power of 2
    pairwise_collision_rule* first_free_collision_rule; //NOTE: free store from previously deleted collision rules

    f32 time;
};

//TODO(fran): check what casey said about adding static to functions to reduce link/compilation time (explanation in day 53, min ~1:05:00)

