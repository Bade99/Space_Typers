#pragma once

//Following on Casey (Molly Rocket) 's idea of multiplatform design this are the Services that the Game provides to the Platform Layer

#include <cstdint> //uint8_t

#define _USE_MATH_DEFINES //M_PI
#include <math.h> //sinf

#if _DEBUG
#define game_assert(expression) if(!(expression)){*(int*)0=0;}
#else
#define game_assert(expression) 
#endif

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

struct game_framebuffer {
    void* bytes{ 0 };
    i32 width;
    i32 height;
    i32 bytes_per_pixel;
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

struct game_controller_input {
    utf16 new_char; //probably not good to use utf16 with the surrogate pair situation, utf32 might be better for just getting one clean character no matter what
    union {
        game_button_state buttons[6]; //REMEMBER: actualizar el tamaño del array si agrego botones
#pragma warning(push) //TODO(fran): simpler is to just use compiler flags, also Im not sure every compiler accepts #pragma
#pragma warning(disable : 4201)
        struct {
            game_button_state up;
            game_button_state down;
            game_button_state left;
            game_button_state right;
            game_button_state enter;
            game_button_state back;
            //game_button_state escape;
        };
#pragma warning( pop )
    };
};

struct game_input {
    game_controller_input controller; //TODO(fran): multiple controllers, probably pointless
};

struct v2 {
    float x, y;
};

struct Word {
    v2 pos;
    v2 speed;
    utf16 txt[25];
};

struct game_world {
    int width, height;
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

struct game_state {
    int xoff;
    int yoff;
    int hz;
};

struct game_memory { //We are gonna be taking the handmade hero route, to see how it goes and if it is something that I like when the thing gets complex
    u32 permanent_storage_sz;
    void* permanent_storage; //NOTE: REQUIRED to be cleared to zero on startup
    u32 transient_storage_sz;
    void* transient_storage; //NOTE: REQUIRED to be cleared to zero on startup

    bool is_initialized=false;
};

void game_update_and_render(game_memory* memory, game_framebuffer* frame_buf, game_soundbuffer* sound_buf, game_input* input); //needs timing, input, bitmap buffer, sound buffer

//This are the Services that the Platform Layer provides to the Game
//TODO(fran): this is just debug code, doesnt handle many problems
std::pair<void*,u32> platform_read_entire_file(const char* filename);
bool platform_write_entire_file(const char* filename, void* memory, u32 mem_sz);
void platform_free_file_memory(void* memory);
