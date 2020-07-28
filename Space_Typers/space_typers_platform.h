#pragma once
#include <cstdint> //uint8_t


typedef uint8_t u8; //prepping for jai //TODO(fran): move to space_typers_base_types.h? or something like that
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

#include "space_typers_vector.h" //TODO(fran): it's ugly to just have this guy here for v3_i32 in the controller, and we also have to put it after the typedefs

struct game_memory { //We are gonna be taking the handmade hero route, to see how it goes and if it is something that I like when the thing gets complex
    u32 permanent_storage_sz;
    void* permanent_storage; //NOTE: REQUIRED to be cleared to zero on startup
    u32 transient_storage_sz;
    void* transient_storage; //NOTE: REQUIRED to be cleared to zero on startup

    bool is_initialized = false;
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

struct game_input {
    game_controller_input controller; //TODO(fran): multiple controllers, probably pointless
    f32 dt_sec;
};

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


//Following on Casey (Molly Rocket) 's idea of multiplatform design:

//INFO IMPORTANT: this are the Services that the Game provides to the Platform Layer
//INFO: for now this has to be very fast, < 1ms
void game_get_sound_samples(game_memory* memory, game_soundbuffer* sound_buf);

void game_update_and_render(game_memory* memory, game_framebuffer* frame_buf, game_input* input); //needs timing, input, bitmap buffer, sound buffer

//INFO IMPORTANT: This are the Services that the Platform Layer provides to the Game
//TODO(fran): this is just debug code, doesnt handle many problems
struct platform_read_entire_file_res { void* mem; u32 sz; };
platform_read_entire_file_res platform_read_entire_file(const char* filename); //TODO(fran): probably simpler is to create a struct with the two members instead of using pair
bool platform_write_entire_file(const char* filename, void* memory, u32 mem_sz);
void platform_free_file_memory(void* memory);