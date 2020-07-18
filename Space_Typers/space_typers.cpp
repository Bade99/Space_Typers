#include "space_typers.h"


void Render(game_framebuffer* buf, int xoff, int yoff) {
    u8* row = (u8*)buf->bytes;
    int pitch = buf->width * buf->bytes_per_pixel;
    for (int y = 0; y < buf->height; y++) {
        u32* pixel = (u32*)row;
        for (int x = 0; x < buf->width; x++) {
            //AARRGGBB
            *pixel++ = ((u8)(y + yoff) << 8) | (u8)(x + xoff);
        }
        row += pitch;//he does this instead of just incrementing each time in the x loop because of alignment things I dont care about now
    }
}

void OutputSound(game_soundbuffer* sound_buf, int hz) {
    static f32 sine_t = 0;
    int volume = 3000;
    int waveperiod = sound_buf->samples_per_sec / hz;

    i16* sample_out = sound_buf->samples;
    for (int sample_index = 0; sample_index < sound_buf->sample_count_to_output; sample_index++) {
        f32 sine = sinf(sine_t);
        i16 sample_val = (i16)(sine * volume);
        *sample_out++ = sample_val;
        *sample_out++ = sample_val;
        sine_t += (1.f / (f32)waveperiod) * 2.f * (f32)M_PI; //Maintain position in the sine wave, otherwise on frequency change we have a discontinuity
    }
}

void game_update_and_render(game_memory* memory, game_framebuffer* frame_buf, game_soundbuffer* sound_buf, game_input* input) {
    game_assert(sizeof(game_state) <= memory->permanent_storage_sz);
    game_state* game_st = (game_state*)memory->permanent_storage;
    if (!memory->is_initialized) {
        game_st->hz = 256;

        auto [background_bmp_mem, background_bmp_sz] = platform_read_entire_file(__FILE__);
        if (background_bmp_mem) {
            platform_write_entire_file("C:/Users/Brenda-Vero-Frank/Desktop/testfile.txt",background_bmp_mem,background_bmp_sz);
            platform_free_file_memory(background_bmp_mem);
        }

        memory->is_initialized = true; //TODO(fran): should the platform layer do this?
    }

    if (input->controller.up.ended_down) { game_st->yoff += 5; } //REMEMBER: you get the input for the previous frame
    if (input->controller.down.ended_down) { game_st->yoff -= 5; } //TODO(fran): axis (0,0) should be the bottom left corner, x positive to the right and y positive up
    if (input->controller.right.ended_down) { game_st->xoff += 5; }
    if (input->controller.left.ended_down) { game_st->xoff -= 5; }
    
    OutputSound(sound_buf,game_st->hz); //TODO(fran): allow sample offsets, we could need to re-write stuff or advance further than where we currently are
    Render(frame_buf, game_st->xoff, game_st->yoff);
}