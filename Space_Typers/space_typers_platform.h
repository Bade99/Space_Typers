#pragma once
#include <cstdint> //uint8_t

//NOTE: Indicates whether the build is developer only, in which case all sorts of garbage, testing, counters, etc etc are allowed
#define _DEVELOPER

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
typedef char32_t utf32;//TODO(fran): use utf32?

#define Bytes(n) (n)
#define Kilobytes(n) ((n)*1024)
#define Megabytes(n) (Kilobytes(n)*1024)
#define Gigabytes(n) (Megabytes(n)*1024LL)
#define Terabytes(n) (Gigabytes(n)*1024LL)

//TODO(fran): this and the debug_cycle_counter stuff should go in space_typers.h if we arent going to pass this info to the platform layer
#define arr_count(arr) sizeof(arr)/sizeof((arr)[0])

#define zero_struct(instance) zero_mem(&(instance),sizeof(instance))
void zero_mem(void* ptr, u32 sz) {
    //TODO(fran): performance
    u8* bytes = (u8*)ptr;
    while (sz--)
        *bytes++ = 0;
}

#include "space_typers_vector.h" //TODO(fran): it's ugly to just have this guy here for v3_i32 in the controller, and we also have to put it after the typedefs

#ifdef _DEVELOPER

struct debug_cycle_counter {
    u64 cycles_elapsed;
    u32 hit_count;
    char name[32];
};

enum
{
    /* 0 */ debug_cycle_counter_game_update_and_render,
    /* 1 */ debug_cycle_counter_output_render_group,
    /* 2 */ debug_cycle_counter_game_render_rectangle,
    /* 3 */ debug_cycle_counter_process_pixel,
    /* 4 */ debug_cycle_counter_game_render_rectangle_fast,

    /* n+1 */ debug_cycle_counter_COUNT,
};

debug_cycle_counter global_cycle_counter[debug_cycle_counter_COUNT];

void handle_global_cycle_counter() {
    printf("CYCLE COUNTS:\n");
    for (u32 i = 0; i < arr_count(global_cycle_counter); i++)
    //for (i32 i = arr_count(global_cycle_counter)-1; i >= 0 ; i--)
        if(global_cycle_counter[i].hit_count)
        printf("[%32s]: %llu cycles | %u hits | %llu cy/ht\n", global_cycle_counter[i].name, global_cycle_counter[i].cycles_elapsed, global_cycle_counter[i].hit_count, global_cycle_counter[i].cycles_elapsed / (u64)global_cycle_counter[i].hit_count);
    //TODO(fran): maybe do the reset here
}

#if _MSC_VER
#define RESET_TIMED_BLOCKS zero_struct(global_cycle_counter);
#define START_TIMED_BLOCK(id) u64 cycle_start##id = __rdtsc();
#define END_TIMED_BLOCK(id) global_cycle_counter[debug_cycle_counter_##id] = { global_cycle_counter[debug_cycle_counter_##id].cycles_elapsed + __rdtsc() - cycle_start##id,global_cycle_counter[debug_cycle_counter_##id].hit_count+1,#id};
#define END_TIMED_BLOCK_COUNTED(id,count) global_cycle_counter[debug_cycle_counter_##id] = { global_cycle_counter[debug_cycle_counter_##id].cycles_elapsed + __rdtsc() - cycle_start##id,global_cycle_counter[debug_cycle_counter_##id].hit_count+count,#id};
//NOTE: the idea of the _COUNTED macro is that you can approximate a similar value to using the regular _TIMED_BLOCK but without calling it on each iteration and thus reducing cache pollution
#else

#endif

#else
void handle_global_cycle_counter(){}
#define RESET_TIMED_BLOCKS 
#define START_TIMED_BLOCK(id) 
#define END_TIMED_BLOCK(id) 
#define END_TIMED_BLOCK_COUNTED(id,count) 
#endif

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
    v3 mouse; //NOTE: z identifies the mouse wheel value

    game_button_state back; //TODO(fran): I think that for the type of game I want to make no key needs to be persistent as I do it now, they just need a boolean is_on_repeat that persists

    game_button_state enter;

    union { //TODO(fran): check LLVM supports nameless struct/union
        game_button_state persistent_buttons[4]; //REMEMBER: actualizar el tamaño del array si agrego botones
        struct {
            game_button_state up;
            game_button_state down;
            game_button_state left;
            game_button_state right;
            //game_button_state escape;
        };
    };
};

struct game_input {
    game_controller_input controller; //TODO(fran): multiple controllers, probably pointless
    f32 dt_sec;
};

struct game_framebuffer { //NOTE: must have 4 bytes per pixel
    void* mem{ 0 };
    i32 width;
    i32 height;
    i32 pitch;
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