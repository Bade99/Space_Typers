#pragma once

#include <cstdint> //uint8_t

#define _USE_MATH_DEFINES //M_PI
#include <math.h> //sinf

#include<utility> //std::pair

#include "space_typers_img.h" //TODO(fran): create separate .h for game specific things

#if _DEBUG //TODO(fran): change to my own flag and set it with each compiler's flags, or even simpler just let the user set the flag
#define game_assert(expression) if(!(expression)){*(int*)0=0;}
#else
#define game_assert(expression) 
#endif

#define sizeof_arr(arr) sizeof(arr)/sizeof((arr)[0])

//TODO(fran): separate the stuff that the platform needs to know from what it does not in two different .h files

typedef uint8_t u8; //prepping for jai
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef wchar_t utf16;

struct argb_f32 { f32 a, r, g, b; };

#include "space_typers_vectors.h"

struct game_framebuffer {
    void* bytes{ 0 };
    i32 width;
    i32 height;
    i32 bytes_per_pixel;
    i32 pitch;
    //TODO(fran): add pitch?
};

struct game_soundbuffer {
    i16* samples;
    int sample_count_to_output; //Number of samples to fill each time
    int samples_per_sec;
};

struct game_button_state {
    int half_transition_count;
    bool ended_down;
};

struct game_controller_input {//TODO(fran): this struct probably needs rearranging for better packing
    utf16 new_char; //probably not good to use utf16 with the surrogate pair situation, utf32 might be better for just getting one clean character no matter what

    game_button_state mouse_buttons[3]; //INFO: left,right,middle
    v3_i32  mouse; //NOTE: z identifies the mouse wheel value

    game_button_state back; //TODO(fran): I think that for the type of game I want to make no key needs to be persistent as I do it now, they just need a boolean is_on_repeat that persists

    union { //TODO(fran): check LLVM supports nameless struct/union
        game_button_state persistent_buttons[5]; //REMEMBER: actualizar el tamaño del array si agrego botones
        struct {
            game_button_state up;
            game_button_state down;
            game_button_state left;
            game_button_state right;
            game_button_state enter;
            //game_button_state escape;
        };
    };
};

struct img {
    int width, height, channels, bytes_per_channel;
    //v2_i32 align; //TODO(fran): add align offset to change the position of the bitmap relative to a point (Handmade hero day 39)
    void* mem;
};

struct game_input {
    game_controller_input controller; //TODO(fran): multiple controllers, probably pointless
    f32 dt_sec;
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
    v2_f32 pos;
    v2_f32 radius;
};

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
};

struct game_memory { //We are gonna be taking the handmade hero route, to see how it goes and if it is something that I like when the thing gets complex
    u32 permanent_storage_sz;
    void* permanent_storage; //NOTE: REQUIRED to be cleared to zero on startup
    u32 transient_storage_sz;
    void* transient_storage; //NOTE: REQUIRED to be cleared to zero on startup

    bool is_initialized=false;
};

//Following on Casey (Molly Rocket) 's idea of multiplatform design:

//INFO IMPORTANT: this are the Services that the Game provides to the Platform Layer
//INFO: for now this has to be very fast, < 1ms
void game_get_sound_samples(game_memory* memory, game_soundbuffer* sound_buf);

void game_update_and_render(game_memory* memory, game_framebuffer* frame_buf, game_input* input); //needs timing, input, bitmap buffer, sound buffer

//INFO IMPORTANT: This are the Services that the Platform Layer provides to the Game
//TODO(fran): this is just debug code, doesnt handle many problems
std::pair<void*,u32> platform_read_entire_file(const char* filename); //TODO(fran): probably simpler is to create a struct with the two members instead of using pair
bool platform_write_entire_file(const char* filename, void* memory, u32 mem_sz);
void platform_free_file_memory(void* memory);
