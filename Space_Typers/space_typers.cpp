#include "space_typers.h"

//REMAINDERS:
//·optimization fp:fast
//·optimization (google) intel intrinsics guide


//void game_render(game_framebuffer* buf, int xoff, int yoff) {
//    u8* row = (u8*)buf->bytes;
//    int pitch = buf->width * buf->bytes_per_pixel;
//    for (int y = 0; y < buf->height; y++) {
//        u32* pixel = (u32*)row;
//        for (int x = 0; x < buf->width; x++) {
//            //AARRGGBB
//            *pixel++ = ((u8)(y + yoff) << 8) | (u8)(x + xoff);
//        }
//        row += pitch;//he does this instead of just incrementing each time in the x loop because of alignment things I dont care about now
//    }
//}

//void game_output_sound(game_soundbuffer* sound_buf, int hz) {
//    static f32 sine_t = 0;
//    int volume = 3000;
//    int waveperiod = sound_buf->samples_per_sec / hz;
//
//    i16* sample_out = sound_buf->samples;
//    for (int sample_index = 0; sample_index < sound_buf->sample_count_to_output; sample_index++) {
//        f32 sine = sinf(sine_t);
//        i16 sample_val = (i16)(sine * volume);
//        *sample_out++ = sample_val;
//        *sample_out++ = sample_val;
//        sine_t += (1.f / (f32)waveperiod) * 2.f * (f32)M_PI; //Maintain position in the sine wave, otherwise on frequency change we have a discontinuity
//
//        if (sine_t > (2.f * (f32)M_PI)) sine_t -= (2.f * (f32)M_PI); //INFO REMEMBER: great visualization of float losing precision, if you dont keep it between 0 and 2pi sooner or later the sine starts to be wrong
//
//    }
//}

void game_get_sound_samples(game_memory* memory, game_soundbuffer* sound_buf) {
    //game_assert(sizeof(game_state) <= memory->permanent_storage_sz);
    //game_state* game_st = (game_state*)memory->permanent_storage;
    //game_output_sound(sound_buf, game_st->hz);
}

//void game_render_rectangle(game_framebuffer* buf, v2_i32 pos, v2_i32 sz, u32 color) {
//    if (pos.x + sz.x > buf->width || pos.x<0 || pos.y + sz.y>buf->height || pos.y < 0) return;
//    u8* row = (u8*)buf->bytes;
//    row += pos.y * buf->pitch + pos.x*buf->bytes_per_pixel;
//    for (int y = 0; y < sz.y; y++) {
//        u32* pixel = (u32*)row;
//        for (int x = 0; x < sz.x; x++) {
//            //AARRGGBB
//            *pixel++ = color;
//        }
//        row += buf->pitch;//he does this instead of just incrementing each time in the x loop because of alignment things I dont care about now
//    }
//}

i32 round_f32_to_i32(f32 n) {
    //TODO(fran): intrinsic
    i32 res = (i32)(n+.5f); //TODO(fran): does this work correctly with negative numbers?
    return res;
}

void game_render_rectangle(game_framebuffer* buf, v2_f32 minf, v2_f32 maxf, argb_f32 colorf) {
    //The Rectangles are filled NOT including the final row/column
    v2_i32 min = { round_f32_to_i32(minf.x), round_f32_to_i32(minf.y) };
    v2_i32 max = { round_f32_to_i32(maxf.x), round_f32_to_i32(maxf.y) };

    if (min.x < 0)min.x = 0;
    if (min.y < 0)min.y = 0;
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;

    u32 col = (round_f32_to_i32(colorf.a*255.f) <<24) |
              (round_f32_to_i32(colorf.r * 255.f) <<16) |
              (round_f32_to_i32(colorf.g * 255.f) << 8) |
              round_f32_to_i32(colorf.b * 255.f);

    u8* row = (u8*)buf->bytes + min.y * buf->pitch + min.x * buf->bytes_per_pixel;
    for (int y = min.y; y < max.y; y++) {
        u32* pixel = (u32*)row;
        for (int x = min.x; x < max.x; x++) {
            //AARRGGBB
            *pixel++ = col;
        }
        row += buf->pitch;//he does this instead of just incrementing each time in the x loop because of alignment things I dont care about now
    }
}

bool game_check_collision_point_to_rc(v2_f32 p, rc r) {
    return p.x > (r.pos.x-r.radius.x)&& p.x < (r.pos.x + r.radius.x) && p.y >(r.pos.y - r.radius.y) && p.y < (r.pos.y + r.radius.y);
}

png DEBUG_load_png(const char* filename) {
    png res{0};
    auto [img_mem, img_sz] = platform_read_entire_file(filename);
    if (img_mem) {
        //platform_write_entire_file("C:/Users/Brenda-Vero-Frank/Desktop/testfile.txt",background_bmp_mem,background_bmp_sz);
        int width, height, channels;
        u8* bytes = stbi_load_from_memory((u8*)img_mem, img_sz, &width, &height, &channels, 0); //NOTE: this interface always assumes 8 bits per component
        platform_free_file_memory(img_mem);
        if (bytes) {
            res.mem = bytes;
            res.width = width;
            res.height = height;
            res.channels = channels;
            res.bytes_per_channel = 1; //NOTE: this interface always assumes 8 bits per component

            u8* img_row = (u8*)res.mem; //INFO: R and B values need to be swapped
            int img_pitch = res.width * res.channels * res.bytes_per_channel;
            for (int y = 0; y < res.height; y++) {
                u32* img_pixel = (u32*)img_row;
                for (int x = 0; x < res.width; x++) {
                    //AARRGGBB
                    *img_pixel++ = (*img_pixel & 0xFF00FF00) | ((*img_pixel & 0x00FF0000)>>16) | ((*img_pixel & 0x000000FF) << 16); //TODO(fran): learn more about this, there are probably simpler and better ways to do it
                }
                img_row += img_pitch; //NOTE: now I starting seeing the benefits of using pitch, very easy to change rows when you only have to display parts of the img
            }


        }
    }
    return res;
}

void DEBUG_unload_png(png* img) {
    stbi_image_free(img->mem);
    //TODO(fran): zero the other members?
}

void game_render_img(game_framebuffer* buf, v2_f32 pos, png* img) {

    v2_i32 min = { round_f32_to_i32(pos.x), round_f32_to_i32(pos.y) };
    v2_i32 max = { round_f32_to_i32(pos.x+img->width), round_f32_to_i32(pos.y+img->height) };

    if (min.x < 0)min.x = 0;
    if (min.y < 0)min.y = 0;
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;

    u8* row = (u8*)buf->bytes + min.y * buf->pitch + min.x * buf->bytes_per_pixel;
    u8* img_row = (u8*)img->mem;
    int img_pitch = img->width * img->channels * img->bytes_per_channel;
    for (int y = min.y; y < max.y; y++) {
        u32* pixel = (u32*)row;
        u32* img_pixel = (u32*)img_row;
        for (int x = min.x; x < max.x; x++) {
            //AARRGGBB
            *pixel++ = *img_pixel++;
        }
        row += buf->pitch;
        img_row += img_pitch; //NOTE: now I starting seeing the benefits of using pitch, very easy to change rows when you only have to display parts of the img
    }

}

void game_update_and_render(game_memory* memory, game_framebuffer* frame_buf, game_input* input) {
    game_assert(sizeof(game_state) <= memory->permanent_storage_sz);
    game_state* game_st = (game_state*)memory->permanent_storage;
    
    //INFO IDEA: I see the game, we load stages from the filesystem, each stage can contain stages or levels inside, we show them in a nice rounded squares type menu, you can click to enter any stage/level, then you are sent to the real level

    //NOTE:since our worlds are going to be very small we wont need to do virtualized space like Handmade Hero (day 33), but the idea is there if we decide to reuse the engine for something bigger

    //IDEA: pixel art assets so we can create them fast, Im not sure how well they'll look next to the text for the words, maybe some font can fix that

    //IDEA NOTE: text autocomplete makes you worse at typing, we want to train people to be able to write everything themselves

    //NOTE IDEA: I see an aesthetic with oranges and white, warm colors, reds

    //game_level_map lvl_map;
    //lvl_map.origin = { 0,0 };
    //lvl_map.unit = 1;

    //IDEA: different visible walls for the words to collide against, so it creates sort of a priority system where some words must be typed first cause they are going to collide with a wall that is much closer than the one on the other side of the screen

    if (!memory->is_initialized) {
        //game_st->hz = 256;

        game_st->word_height_meters = 1.5f;//TODO(fran): maybe this would be better placed inside world
        game_st->word_height_pixels = 60;

        game_st->lower_left_pixels = { 0.f,(f32)frame_buf->height };

        game_st->word_meters_to_pixels = (f32)game_st->word_height_pixels / game_st->word_height_meters; //Transform from meters to pixels

        initialize_arena(&game_st->memory_arena, (u8*)memory->permanent_storage + sizeof(game_state), memory->permanent_storage_sz - sizeof(game_state));

        game_st->world.stages = push_mem(&game_st->memory_arena,game_stage);
        game_st->world.stage_count = 1;
        game_st->world.current_stage = 0;

        game_st->world.stages[0].lvls = push_arr(&game_st->memory_arena, game_level_map, 2);
        game_st->world.stages[0].level_count = 2; //TODO(fran): can we use sizeof_arr ?? I doubt it
        game_st->world.stages[0].current_lvl = 0;

        game_st->world.stages[0].lvls[0].walls = push_mem(&game_st->memory_arena,colored_rc);//TODO(fran): wall struct?
        game_st->world.stages[0].lvls[0].words = push_mem(&game_st->memory_arena, colored_rc);//TODO(fran): word struct?
        game_st->world.stages[0].lvls[0].wall_count = 1;
        game_st->world.stages[0].lvls[0].word_count = 1;

        game_st->world.stages[0].lvls[1].walls = push_mem(&game_st->memory_arena, colored_rc);
        game_st->world.stages[0].lvls[1].words = push_mem(&game_st->memory_arena, colored_rc);
        game_st->world.stages[0].lvls[1].wall_count = 1;
        game_st->world.stages[0].lvls[1].word_count = 1;

        game_st->world.stages[0].lvls[0].walls[0].rect.pos = { 10.5f * game_st->word_height_meters, 9.5f * game_st->word_height_meters }; //TODO(fran): allocate space for all these
        game_st->world.stages[0].lvls[0].walls[0].rect.radius = { .5f * game_st->word_height_meters , 4.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[0].walls[0].color = { 0.f,1.f,0.f,0.f };

        game_st->world.stages[0].lvls[0].words[0].rect.pos = { 16.5f * game_st->word_height_meters , 10.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[0].words[0].rect.radius = { .5f * game_st->word_height_meters, .5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[0].words[0].color = { 0.f,1.f,1.f,1.f };

        game_st->world.stages[0].lvls[1].walls[0].rect.pos = { 8.5f * game_st->word_height_meters, 7.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].walls[0].rect.radius = { .5f * game_st->word_height_meters , 4.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].walls[0].color = { 0.f,1.f,0.f,0.f };

        game_st->world.stages[0].lvls[1].words[0].rect.pos = { 16.5f * game_st->word_height_meters , 10.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].words[0].rect.radius = { .5f * game_st->word_height_meters, .5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].words[0].color = { 0.f,0.f,1.f,1.f };

        game_st->DEBUG_background = DEBUG_load_png("assets/img/dark_night_sky.png"); //TODO(fran): release mem DEBUG_unload_png(&background);

        memory->is_initialized = true; //TODO(fran): should the platform layer do this?
    }

    //TODO(fran):objects may need z position to resolve conflicts with overlapping

    //if (input->controller.up.ended_down) { game_st->yoff += 5; } //REMEMBER: you get the input for the previous frame
    //if (input->controller.down.ended_down) { game_st->yoff -= 5; } //TODO(fran): axis (0,0) should be the bottom left corner, x positive to the right and y positive up
    //if (input->controller.right.ended_down) { game_st->xoff += 5; }
    //if (input->controller.left.ended_down) { game_st->xoff -= 5; }
    
    //game_render(frame_buf, game_st->xoff, game_st->yoff);
    //argb_f32 player_color = { 0,1.f,1.f,1.f};
    //if (input->controller.mouse_buttons[0].ended_down) player_color = { 0,1.f,0.f,0.f };
    //if(input->controller.mouse_buttons[1].ended_down) player_color = { 0,0.f,1.f,0.f };
    //if (input->controller.mouse_buttons[2].ended_down) player_color = { 0,0.f,0.f,1.f };

    if (input->controller.back.ended_down) {
        game_st->world.stages[0].current_lvl++;
        game_st->world.stages[0].current_lvl %= game_st->world.stages[0].level_count;
    }

    //IDEA: what if the platform layer gave you scaling information, so you set up your render like you want and then the only thing that needs to change is scaling

    game_level_map* current_level = &game_st->world.stages[0].lvls[game_st->world.stages[0].current_lvl];

    v2_f32 d_word = { 0,0 };
    f32 velocity = 5.f * game_st->word_height_meters * input->dt_sec;
    if (input->controller.right.ended_down) {
        d_word.x = +velocity;
    }
    if (input->controller.left.ended_down) {
        d_word.x = -velocity ;
    }
    if (input->controller.up.ended_down) {
        d_word.y = +velocity;
    }
    if (input->controller.down.ended_down) {
        d_word.y = -velocity;
    }
    {
        v2_f32 new_pos = { current_level->words[0].rect.pos.x + d_word.x,current_level->words[0].rect.pos.y + d_word.y };
        v2_f32 new_word_pos00{ new_pos.x - current_level->words[0].rect.radius.x,new_pos.y - current_level->words[0].rect.radius.y};
        v2_f32 new_word_pos01{ new_pos.x + current_level->words[0].rect.radius.x,new_pos.y - current_level->words[0].rect.radius.y};
        v2_f32 new_word_pos10{ new_pos.x - current_level->words[0].rect.radius.x,new_pos.y + current_level->words[0].rect.radius.y};
        v2_f32 new_word_pos11{ new_pos.x + current_level->words[0].rect.radius.x,new_pos.y + current_level->words[0].rect.radius.y}; //TODO(fran): rc to rc collision

        if (!game_check_collision_point_to_rc(new_word_pos00, current_level->walls[0].rect) &&
            !game_check_collision_point_to_rc(new_word_pos01, current_level->walls[0].rect)&&
            !game_check_collision_point_to_rc(new_word_pos10, current_level->walls[0].rect)&&
            !game_check_collision_point_to_rc(new_word_pos11, current_level->walls[0].rect)) { //TODO(fran): add collision checks for the mid points, if two objs are the same width for example then when they line up perfectly one can enter the other from the top or bottom. Or just move on to a new technique
            //TODO(fran): fix faster diagonal movement once we properly use vectors

           current_level->words[0].rect.pos.x += d_word.x;//TODO(fran): more operator overloading
           current_level->words[0].rect.pos.y += d_word.y;

        }
    }

    game_render_rectangle(frame_buf, { (f32)0,(f32)0 }, { (f32)frame_buf->width,(f32)frame_buf->height }, { 0,.5f,0.f,.5f });
    //game_render_rectangle(frame_buf, { (f32)input->controller.mouse.x,(f32)input->controller.mouse.y }, {(f32)input->controller.mouse.x + 16, (f32)input->controller.mouse.y+16 }, player_color);

    game_render_img(frame_buf, { -100,10 }, &game_st->DEBUG_background); //TODO(fran): correct clipping when leaving the screen from the left or top
    game_render_img(frame_buf, { 1500,300 }, &game_st->DEBUG_background);
    game_render_img(frame_buf, { 500,-100 }, &game_st->DEBUG_background); //TODO(fran): correct clipping when leaving the screen from the left or top
    game_render_img(frame_buf, { 500,800 }, &game_st->DEBUG_background); //TODO(fran): correct clipping when leaving the screen from the left or top
    //NOTE or TODO(fran): stb gives the png in correct orientation by default, I'm not sure whether that's gonna cause problems with our orientation reversing

    //NOTE: now when we go to render we have to transform from meters, the unit everything in our game is, to pixels, the unit of the screen

    v2_f32 wall_min = { game_st->lower_left_pixels.x + (current_level->walls[0].rect.pos.x - current_level->walls[0].rect.radius.x) * game_st->word_meters_to_pixels, game_st->lower_left_pixels.y - (current_level->walls[0].rect.pos.y + current_level->walls[0].rect.radius.y) * game_st->word_meters_to_pixels };
    v2_f32 wall_max = { game_st->lower_left_pixels.x + (current_level->walls[0].rect.pos.x + current_level->walls[0].rect.radius.x) * game_st->word_meters_to_pixels, game_st->lower_left_pixels.y - (current_level->walls[0].rect.pos.y - current_level->walls[0].rect.radius.y) * game_st->word_meters_to_pixels };

    game_render_rectangle(frame_buf, wall_min, wall_max, current_level->walls[0].color);

    v2_f32 word_min = { game_st->lower_left_pixels.x + (current_level->words[0].rect.pos.x - current_level->words[0].rect.radius.x)* game_st->word_meters_to_pixels, game_st->lower_left_pixels.y - (current_level->words[0].rect.pos.y + current_level->words[0].rect.radius.y)* game_st->word_meters_to_pixels };
    v2_f32 word_max = { game_st->lower_left_pixels.x + (current_level->words[0].rect.pos.x + current_level->words[0].rect.radius.x)* game_st->word_meters_to_pixels, game_st->lower_left_pixels.y - (current_level->words[0].rect.pos.y - current_level->words[0].rect.radius.y) * game_st->word_meters_to_pixels };
    //INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left

    game_render_rectangle(frame_buf, word_min, word_max, current_level->words[0].color);
}
