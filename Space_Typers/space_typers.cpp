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

bool game_check_collision_point_to_rc(v2_f32 p, rc r) {
    return p.x > (r.pos.x-r.radius.x)&& p.x < (r.pos.x + r.radius.x) && p.y >(r.pos.y - r.radius.y) && p.y < (r.pos.y + r.radius.y);
}

img DEBUG_load_png(const char* filename) {
    img res{0};
    auto [img_mem, img_sz] = platform_read_entire_file(filename);
    if (img_mem) {
        //platform_write_entire_file("C:/Users/Brenda-Vero-Frank/Desktop/testfile.txt",background_bmp_mem,background_bmp_sz);
        int width, height, channels;
        u8* bytes = stbi_load_from_memory((u8*)img_mem, img_sz, &width, &height, &channels, 0); //NOTE: this interface always assumes 8 bits per component
        platform_free_file_memory(img_mem);
        if (bytes) {
            game_assert(channels == 4);
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
            //TODO(fran): get align x and y

        }
    }
    return res;
}

void DEBUG_unload_png(img* image) {
    stbi_image_free(image->mem);
    //TODO(fran): zero the other members?
}

void game_render_img_ignore_transparency(game_framebuffer* buf, v2_f32 pos, img* image) {

    v2_i32 min = { round_f32_to_i32(pos.x), round_f32_to_i32(pos.y) };
    v2_i32 max = { round_f32_to_i32(pos.x + image->width), round_f32_to_i32(pos.y + image->height) };

    int offset_x = 0;
    int offset_y = 0;

    if (min.x < 0) { offset_x = -min.x;  min.x = 0; }
    if (min.y < 0) { offset_y = -min.y; min.y = 0; }
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;

    int img_pitch = image->width * image->channels * image->bytes_per_channel;

    u8* row = (u8*)buf->bytes + min.y * buf->pitch + min.x * buf->bytes_per_pixel;
    u8* img_row = (u8*)image->mem + offset_x * image->channels * image->bytes_per_channel + offset_y * img_pitch;

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

void game_render_img(game_framebuffer* buf, v2_f32 pos, img* image) {

    v2_i32 min = { round_f32_to_i32(pos.x), round_f32_to_i32(pos.y) };
    v2_i32 max = { round_f32_to_i32(pos.x+ image->width), round_f32_to_i32(pos.y+ image->height) };

    int offset_x=0;
    int offset_y=0;

    if (min.x < 0) { offset_x = -min.x ;  min.x = 0; }
    if (min.y < 0) { offset_y = -min.y; min.y = 0; }
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;

    int img_pitch = image->width * image->channels * image->bytes_per_channel;

    u8* row = (u8*)buf->bytes + min.y * buf->pitch + min.x * buf->bytes_per_pixel;
    u8* img_row = (u8*)image->mem + offset_x* image->channels* image->bytes_per_channel + offset_y*img_pitch ;
    
    for (int y = min.y; y < max.y; y++) {
        u32* pixel = (u32*)row;
        u32* img_pixel = (u32*)img_row;
        for (int x = min.x; x < max.x; x++) {
            //AARRGGBB

            //very slow linear blend
            f32 s_a = (f32)((*img_pixel >> 24) & 0xFF) / 255.f; //source
            f32 s_r = (f32)((*img_pixel >> 16) & 0xFF);
            f32 s_g = (f32)((*img_pixel >> 8) & 0xFF);
            f32 s_b = (f32)((*img_pixel >> 0) & 0xFF);

            f32 d_r = (f32)((*pixel >> 16) & 0xFF); //dest
            f32 d_g = (f32)((*pixel >> 8) & 0xFF);
            f32 d_b = (f32)((*pixel >> 0) & 0xFF);

            f32 r = (1.f - s_a) * d_r + s_a * s_r; //TODO(fran): what if the dest had an alpha different from 255?
            f32 g = (1.f - s_a) * d_g + s_a * s_g;
            f32 b = (1.f - s_a) * d_b + s_a * s_b;

            *pixel = 0xFF<<24 | round_f32_to_i32(r)<<16 | round_f32_to_i32(g)<<8 | round_f32_to_i32(b)<<0;

            pixel++;
            img_pixel++;
        }
        row += buf->pitch;
        img_row += img_pitch; //NOTE: now I starting seeing the benefits of using pitch, very easy to change rows when you only have to display parts of the img
    }

}

struct min_max_v2_f32 { v2_f32 min, max; }; //NOTE: game_coords means meters
min_max_v2_f32 transform_to_screen_coords(rc game_coords, f32 meters_to_pixels, v2_f32 camera_pixels, v2_f32 lower_left_pixels) {
    v2_f32 min_pixels = { (game_coords.pos.x - game_coords.radius.x) * meters_to_pixels, (game_coords.pos.y + game_coords.radius.y) * meters_to_pixels };

    v2_f32 min_pixels_camera = min_pixels - camera_pixels;

    //INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
    v2_f32 min_pixels_camera_screen = { lower_left_pixels.x + min_pixels_camera.x , lower_left_pixels.y - min_pixels_camera.y };

    v2_f32 max_pixels = { (game_coords.pos.x + game_coords.radius.x) * meters_to_pixels, (game_coords.pos.y - game_coords.radius.y) * meters_to_pixels };

    v2_f32 max_pixels_camera = max_pixels - camera_pixels;

    v2_f32 max_pixels_camera_screen = { lower_left_pixels.x + max_pixels_camera.x , lower_left_pixels.y - max_pixels_camera.y };

    return { min_pixels_camera_screen , max_pixels_camera_screen };
}

void game_render_rectangle(game_framebuffer* buf, min_max_v2_f32 min_max, argb_f32 colorf) {
    //The Rectangles are filled NOT including the final row/column
    v2_i32 min = { round_f32_to_i32(min_max.min.x), round_f32_to_i32(min_max.min.y) };
    v2_i32 max = { round_f32_to_i32(min_max.max.x), round_f32_to_i32(min_max.max.y) };

    if (min.x < 0)min.x = 0;
    if (min.y < 0)min.y = 0;
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;

    u32 col = (round_f32_to_i32(colorf.a * 255.f) << 24) |
        (round_f32_to_i32(colorf.r * 255.f) << 16) |
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

        game_st->world.stages[0].lvls[0].walls[0].rect.pos = { 14.5f * game_st->word_height_meters, 9.5f * game_st->word_height_meters }; //TODO(fran): allocate space for all these
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

        game_st->DEBUG_background = DEBUG_load_png("assets/img/stars.png"); //TODO(fran): release mem DEBUG_unload_png();
        //game_st->DEBUG_background.align = { 20,20 };
        game_st->DEBUG_menu = DEBUG_load_png("assets/img/down.png"); //TODO(fran): release mem DEBUG_unload_png();
        //game_st->DEBUG_menu.align = { 20,20 };
        game_st->DEBUG_mouse = DEBUG_load_png("assets/img/mouse.png"); //TODO(fran): release mem DEBUG_unload_png();

        game_st->camera = { 0 , 0 }; //TODO(fran): place camera in the world, to simplify first we fix it to the middle of the screen

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

    v2_f32 dd_word = { 0,0 }; //accel
    if (input->controller.right.ended_down) {
        dd_word.x = 1.f;
    }
    if (input->controller.left.ended_down) {
        dd_word.x = -1.f;
    }
    if (input->controller.up.ended_down) {
        dd_word.y = +1.f;
    }
    if (input->controller.down.ended_down) {
        dd_word.y = -1.f;
    }
    if (f32 l = lenght_sq(dd_word); l > 1.f) { //Normalizing the vector, diagonal movement is fixed and also takes care of higher values than 1
        dd_word *= (1.f / sqrtf(l));
    }
    f32 constant_accel = 20.f*game_st->word_height_meters; //m/s^2
    dd_word *= constant_accel;
    //TODO(fran): ordinary differential eqs
    dd_word += -3.5f*current_level->words[0].velocity; //basic "drag" //TODO(fran): understand why this doesnt completely eat the acceleration value
#if 0 //old collision detection
    {
        v2_f32 new_pos = .5f*dd_word*powf(input->dt_sec,2.f) + current_level->words[0].velocity * input->dt_sec + current_level->words[0].rect.pos;
        v2_f32 new_word_pos00{ new_pos.x - current_level->words[0].rect.radius.x,new_pos.y - current_level->words[0].rect.radius.y};
        v2_f32 new_word_pos01{ new_pos.x + current_level->words[0].rect.radius.x,new_pos.y - current_level->words[0].rect.radius.y};
        v2_f32 new_word_pos10{ new_pos.x - current_level->words[0].rect.radius.x,new_pos.y + current_level->words[0].rect.radius.y};
        v2_f32 new_word_pos11{ new_pos.x + current_level->words[0].rect.radius.x,new_pos.y + current_level->words[0].rect.radius.y}; //TODO(fran): rc to rc collision

        current_level->words[0].velocity += dd_word * input->dt_sec;

        if (!game_check_collision_point_to_rc(new_word_pos00, current_level->walls[0].rect) &&
            !game_check_collision_point_to_rc(new_word_pos01, current_level->walls[0].rect)&&
            !game_check_collision_point_to_rc(new_word_pos10, current_level->walls[0].rect)&&
            !game_check_collision_point_to_rc(new_word_pos11, current_level->walls[0].rect)) { //TODO(fran): add collision checks for the mid points, if two objs are the same width for example then when they line up perfectly one can enter the other from the top or bottom. Or just move on to a new technique
            //TODO(fran): fix faster diagonal movement once we properly use vectors

           current_level->words[0].rect.pos = new_pos;

           v2_f32 screen_offset = { (f32)frame_buf->width/2,(f32)frame_buf->height/2 };

           game_st->camera = current_level->words[0].rect.pos *game_st->word_meters_to_pixels - screen_offset;
        }
        else {
            v2_f32 wall_normal = {1,0};
            //current_level->words[0].velocity -= 2 * dot(current_level->words[0].velocity, wall_normal)* wall_normal;//bounce
            current_level->words[0].velocity -= 1 * dot(current_level->words[0].velocity, wall_normal)* wall_normal;//go in line with the wall
        }
    }
#else
    //day 45, min 37 aprox, starts with collision detection "in p"
    //day 46 just multiplayer support and entities, nothing on collision
    //in the end we are going go to "search in t" (day 47)
    {
        //we test for the walls that are in our range of motion, and then we take the closest point where we hit (in my case with the limited amount of walls we might as well test all, at least to start)
        v2_f32 pos_delta = .5f*dd_word*powf(input->dt_sec,2.f) + current_level->words[0].velocity * input->dt_sec;//NOTE: we'll start just checking a point
        f32 closest_t=1.f; //we limit the time to the full motion that can occur
        //for each wall...
        colored_rc& wall = current_level->walls[0];
        colored_rc& word = current_level->words[0];
        v2_f32 min_wall = wall.rect.pos - wall.rect.radius;
        v2_f32 max_wall = wall.rect.pos + wall.rect.radius;

        auto test_wall = [](f32 wallx, f32 oldposx, f32 oldposy, f32 deltax, f32 deltay, f32* t_min, f32 miny, f32 maxy) {//TODO(fran): not a great algorithm, we still get stuck sometimes
            if (deltax != 0.f) {
                f32 t_res = (wallx - oldposx) / deltax;
                f32 Y = oldposy + t_res * deltay;
                if (t_res >= 0.f && t_res < *t_min && Y >= miny && Y <= maxy) {
                    f32 epsilon = .001f;
                    *t_min = maximum(.0f, t_res - epsilon); //probably instead of 0 should do epsilon
                }
            }
        };

        test_wall(min_wall.x, word.rect.pos.x, word.rect.pos.y, pos_delta.x, pos_delta.y, &closest_t, min_wall.y, max_wall.y);
        test_wall(max_wall.x, word.rect.pos.x, word.rect.pos.y, pos_delta.x, pos_delta.y, &closest_t, min_wall.y, max_wall.y);
        test_wall(min_wall.y, word.rect.pos.y, word.rect.pos.x, pos_delta.y, pos_delta.x, &closest_t, min_wall.x, max_wall.x);
        test_wall(max_wall.y, word.rect.pos.y, word.rect.pos.x, pos_delta.y, pos_delta.x, &closest_t, min_wall.x, max_wall.x);

        current_level->words[0].rect.pos += pos_delta*closest_t;

        current_level->words[0].velocity += dd_word * input->dt_sec;//TODO
    }
#endif

    game_render_rectangle(frame_buf, { { (f32)0,(f32)0 }, { (f32)frame_buf->width,(f32)frame_buf->height } }, { 0,.5f,0.f,.5f });
    //game_render_rectangle(frame_buf, { (f32)input->controller.mouse.x,(f32)input->controller.mouse.y }, {(f32)input->controller.mouse.x + 16, (f32)input->controller.mouse.y+16 }, player_color);

    //game_render_img(frame_buf, { -100,10 }, &game_st->DEBUG_background); 
    //game_render_img(frame_buf, { 1500,300 }, &game_st->DEBUG_background);
    //game_render_img(frame_buf, { 500,-100 }, &game_st->DEBUG_background); 
    game_render_img_ignore_transparency(frame_buf, { (f32)frame_buf->width/2 - game_st->DEBUG_background.width / 2,(f32)frame_buf->height/2 - game_st->DEBUG_background.height/2 } , &game_st->DEBUG_background);
    //NOTE or TODO(fran): stb gives the png in correct orientation by default, I'm not sure whether that's gonna cause problems with our orientation reversing
    game_render_img(frame_buf, { 0,(f32)frame_buf->height - game_st->DEBUG_menu.height }, &game_st->DEBUG_menu); //NOTE: it's pretty interesting that you can actually see information in the pixels with full alpha, the program that generates them does not put rgb to 0 so you can see "hidden" things

    //NOTE: now when we go to render we have to transform from meters, the unit everything in our game is, to pixels, the unit of the screen

    game_render_rectangle(
        frame_buf, 
        transform_to_screen_coords(current_level->walls[0].rect, game_st->word_meters_to_pixels, game_st->camera, game_st->lower_left_pixels), 
        current_level->walls[0].color);

    game_render_rectangle(
        frame_buf,
        transform_to_screen_coords(current_level->words[0].rect, game_st->word_meters_to_pixels, game_st->camera, game_st->lower_left_pixels),
        current_level->words[0].color);

    game_render_img(frame_buf, { (f32)input->controller.mouse.x,(f32)input->controller.mouse.y }, &game_st->DEBUG_mouse);
}

//TODO(fran): it would be nice to be free of visual studio, so we can do things like live code editing easier, and also simpler for porting
//TODO(fran): it would be nice to be free of visual studio, so we can do things like live code editing easier, and also simpler for porting