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

//bool game_check_collision_point_to_rc(v2_f32 p, rc r) {
//    return p.x > (r.center.x-r.radius.x)&& p.x < (r.center.x + r.radius.x) && p.y >(r.center.y - r.radius.y) && p.y < (r.center.y + r.radius.y);
//}

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
    v2_f32 min_pixels = { (game_coords.center.x - game_coords.radius.x) * meters_to_pixels, (game_coords.center.y + game_coords.radius.y) * meters_to_pixels };

    v2_f32 min_pixels_camera = min_pixels - camera_pixels;

    //INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
    v2_f32 min_pixels_camera_screen = { lower_left_pixels.x + min_pixels_camera.x , lower_left_pixels.y - min_pixels_camera.y };

    v2_f32 max_pixels = { (game_coords.center.x + game_coords.radius.x) * meters_to_pixels, (game_coords.center.y - game_coords.radius.y) * meters_to_pixels };

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

void game_add_entity(game_state* game_st, const game_entity* new_entity) {
    game_assert(game_st->entity_count+1 < sizeof_arr(game_st->entities));
    game_st->entities[game_st->entity_count] = *new_entity;
    game_st->entity_count++;
}

void game_clear_entities(game_state* game_st) {
    game_st->entity_count = 1;//TODO(fran): once again, 1 or 0?
}

bool test_wall(f32 wallx, f32 oldposx, f32 oldposy, f32 deltax, f32 deltay, f32* t_min, f32 miny, f32 maxy){
    if (deltax != 0.f) {
        f32 t_res = (wallx - oldposx) / deltax;
        f32 Y = oldposy + t_res * deltay;
        if (t_res >= 0.f && t_res < *t_min && Y >= miny && Y <= maxy) {
            f32 epsilon = .001f;
            *t_min = maximum(.0f, t_res - epsilon);
            return true;
        }
    }
    return false;
}


void move_entity(game_entity* entity, v2_f32 acceleration, const game_state* game_st, const game_input* input) {
    v2_f32 pos_delta = .5f * acceleration * powf(input->dt_sec, 2.f) + entity->velocity * input->dt_sec;//NOTE: we'll start just checking a point
    entity->velocity += acceleration * input->dt_sec;//TODO: where does this go? //NOTE: Casey put it here, it had it before calculating pos_delta
    rc e = rc_center_radius(entity->pos, entity->radius);
    
    if (entity->collides) {
        for (int attempt = 0; attempt < 4; attempt++) {
            f32 closest_t = 1.f; //we limit the time to the full motion that can occur //TODO(fran): shouldnt this two be reset each time through the entity loop?
            v2_f32 wall_normal{ 0,0 };
            u32 hit_entity_index = 0;
            v2_f32 desired_position = entity->pos + pos_delta;
            for (int entity_index = 1; entity_index < sizeof_arr(game_st->entities); entity_index++)//TODO(fran): what to do, always start from 1?
            {
                const game_entity* possible_collider = &game_st->entities[entity_index];
                if (possible_collider->collides && possible_collider != entity) {
                    rc collider = rc_center_radius(possible_collider->pos, possible_collider->radius);
                    //Following the Minkowski Sum now we reduce the word(player) to a point and expand every other object(wall) by the size of the word, so with no need to change any of the code we get for free collision detection for the entire shape of the word
                    collider.radius += e.radius;
                    v2_f32 min_collider = collider.center - collider.radius;
                    v2_f32 max_collider = collider.center + collider.radius;


                    //TODO: mirar dia 52 min 9:47, él NO usa e.pos acá sino "Rel" que es igual a e.pos-collider.pos
                    if (test_wall(min_collider.x, e.center.x, e.center.y, pos_delta.x, pos_delta.y, &closest_t, min_collider.y, max_collider.y)) {
                        wall_normal = { -1,0 };
                        hit_entity_index = entity_index;
                    }
                    if (test_wall(max_collider.x, e.center.x, e.center.y, pos_delta.x, pos_delta.y, &closest_t, min_collider.y, max_collider.y)) {
                        wall_normal = { 1,0 };
                        hit_entity_index = entity_index;
                    }
                    if (test_wall(min_collider.y, e.center.y, e.center.x, pos_delta.y, pos_delta.x, &closest_t, min_collider.x, max_collider.x)) {
                        wall_normal = { 0,-1 };
                        hit_entity_index = entity_index;
                    }
                    if (test_wall(max_collider.y, e.center.y, e.center.x, pos_delta.y, pos_delta.x, &closest_t, min_collider.x, max_collider.x)) {
                        wall_normal = { 0,1 };
                        hit_entity_index = entity_index;
                    }
                }
            }//NOTE: if we have two collisions we miss all but the last one I guess

            entity->pos += pos_delta * closest_t;
            if (hit_entity_index) {
                entity->velocity -= 1 * dot(entity->velocity, wall_normal) * wall_normal;//go in line with the wall
                pos_delta = desired_position - entity->pos;
                pos_delta -= 1 * dot(pos_delta, wall_normal) * wall_normal;

                game_entity hit_entity = game_st->entities[hit_entity_index]; //TODO(fran): GetEntityIndex function
                //TODO(fran): check if that entity has any behaviour on collision
            }
            else {
                break;
            }
            //if (closest_t < 1.f) current_level->words[0].velocity = { 0,0 }; //REMEMBER: the biggest problem with the player getting stuck was because we werent cancelling it's previous velocity after a collision
        }
    }
    else {
        entity->pos += pos_delta;
    }
}

void game_initialize_entities(game_state* gs, game_level_map lvl, const game_entity* player) {
    game_clear_entities(gs);
    game_add_entity(gs, player); //NOTE: entity index 1 will always be for the player

    for (u32 i = 0; i < lvl.wall_count;i++) {
        game_entity wall;
        wall.collides = true;
        wall.color = lvl.walls[i].color;
        wall.pos = lvl.walls[i].rect.center;
        wall.radius = lvl.walls[i].rect.radius;
        wall.velocity = { 0,0 };
        game_add_entity(gs, &wall);
    }

    for (u32 i = 0; i < lvl.word_count; i++) { //TODO: store word spawner and max words at the same time
        game_entity word;
        word.collides = true;
        word.color = lvl.words[i].color;
        word.pos = lvl.words[i].rect.center;
        word.radius = lvl.words[i].rect.radius;
        word.velocity = { 0,0 };
        game_add_entity(gs, &word);
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

    //IDEA: different visible walls for the words to collide against, so it creates sort of a priority system where some words must be typed first cause they are going to collide with a wall that is much closer than the one on the other side of the screen

    game_entity init_player_entity;
    init_player_entity.collides = true;
    init_player_entity.color = { .0f,.0f,.0f,1.0f };
    init_player_entity.pos = { 11.f ,5.f}; //NOTE: dont care to multiply by meters, also the first time they are not defined yet
    init_player_entity.radius = { .75f, .75f};
    init_player_entity.velocity = {0,0};

    if (!memory->is_initialized) {
        //game_st->hz = 256;
        game_entity null_entity{ 0 };
        game_add_entity(game_st, &null_entity); //NOTE: entity index 0 is reserved as NULL

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

        game_st->world.stages[0].lvls[0].walls[0].rect.center = { 14.5f * game_st->word_height_meters, 9.5f * game_st->word_height_meters }; //TODO(fran): allocate space for all these
        game_st->world.stages[0].lvls[0].walls[0].rect.radius = { .5f * game_st->word_height_meters , 4.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[0].walls[0].color = { 0.f,1.f,0.f,0.f };

        game_st->world.stages[0].lvls[0].words[0].rect.center = { 16.5f * game_st->word_height_meters , 10.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[0].words[0].rect.radius = { .5f * game_st->word_height_meters, .5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[0].words[0].color = { 0.f,1.f,1.f,1.f };

        game_st->world.stages[0].lvls[1].walls[0].rect.center = { 8.5f * game_st->word_height_meters, 7.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].walls[0].rect.radius = { .5f * game_st->word_height_meters , 4.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].walls[0].color = { 0.f,1.f,0.f,0.f };

        game_st->world.stages[0].lvls[1].words[0].rect.center = { 16.5f * game_st->word_height_meters , 10.5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].words[0].rect.radius = { .5f * game_st->word_height_meters, .5f * game_st->word_height_meters };
        game_st->world.stages[0].lvls[1].words[0].color = { 0.f,0.f,1.f,1.f };

        game_st->DEBUG_background = DEBUG_load_png("assets/img/stars.png"); //TODO(fran): release mem DEBUG_unload_png();
        //game_st->DEBUG_background.align = { 20,20 };
        game_st->DEBUG_menu = DEBUG_load_png("assets/img/down.png"); //TODO(fran): release mem DEBUG_unload_png();
        //game_st->DEBUG_menu.align = { 20,20 };
        game_st->DEBUG_mouse = DEBUG_load_png("assets/img/mouse.png"); //TODO(fran): release mem DEBUG_unload_png();

        game_st->camera = { 0 , 0 }; //TODO(fran): place camera in the world, to simplify first we fix it to the middle of the screen

        game_initialize_entities(game_st, game_st->world.stages[0].lvls[game_st->world.stages[0].current_lvl], &init_player_entity);

        memory->is_initialized = true; //TODO(fran): should the platform layer do this?
    }

    //TODO(fran):objects may need z position to resolve conflicts with overlapping

    //REMEMBER: you get the input for the previous frame
    
    if (input->controller.back.ended_down) {
        game_st->world.stages[0].current_lvl++;
        game_st->world.stages[0].current_lvl %= game_st->world.stages[0].level_count;

        game_initialize_entities(game_st, game_st->world.stages[0].lvls[game_st->world.stages[0].current_lvl], &init_player_entity);
    }

    //IDEA: what if the platform layer gave you scaling information, so you set up your render like you want and then the only thing that needs to change is scaling

    game_assert(game_st->entity_count >= 3); //null, player, wall, (actually 4 with the word)
    game_entity& player_entity = game_st->entities[1];

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
    dd_word += -3.5f* player_entity.velocity; //basic "drag" //TODO(fran): understand why this doesnt completely eat the acceleration value

    move_entity(&player_entity, dd_word, game_st, input); //TODO(fran): for each entity that moves

    v2_f32 screen_offset = { (f32)frame_buf->width / 2,(f32)frame_buf->height / 2 };
    game_st->camera = player_entity.pos *game_st->word_meters_to_pixels - screen_offset; //TODO(fran): nicer camera update, lerp

    game_render_rectangle(frame_buf, { { (f32)0,(f32)0 }, { (f32)frame_buf->width,(f32)frame_buf->height } }, { 0,.5f,0.f,.5f });
    //game_render_rectangle(frame_buf, { (f32)input->controller.mouse.x,(f32)input->controller.mouse.y }, {(f32)input->controller.mouse.x + 16, (f32)input->controller.mouse.y+16 }, player_color);

    //game_render_img(frame_buf, { -100,10 }, &game_st->DEBUG_background); 
    //game_render_img(frame_buf, { 1500,300 }, &game_st->DEBUG_background);
    //game_render_img(frame_buf, { 500,-100 }, &game_st->DEBUG_background); 
    game_render_img_ignore_transparency(frame_buf, { (f32)frame_buf->width/2 - game_st->DEBUG_background.width / 2,(f32)frame_buf->height/2 - game_st->DEBUG_background.height/2 } , &game_st->DEBUG_background);
    //NOTE or TODO(fran): stb gives the png in correct orientation by default, I'm not sure whether that's gonna cause problems with our orientation reversing
    game_render_img(frame_buf, { 0,(f32)frame_buf->height - game_st->DEBUG_menu.height }, &game_st->DEBUG_menu); //NOTE: it's pretty interesting that you can actually see information in the pixels with full alpha, the program that generates them does not put rgb to 0 so you can see "hidden" things

    //NOTE: now when we go to render we have to transform from meters, the unit everything in our game is, to pixels, the unit of the screen

    for (u32 i = 1; i < game_st->entity_count; i++) { //TODO: again 1 or 0
        const game_entity* e = &game_st->entities[i];
        game_render_rectangle(
            frame_buf, 
            transform_to_screen_coords({ e->pos , e->radius }, game_st->word_meters_to_pixels, game_st->camera, game_st->lower_left_pixels), 
            e->color);
    }

    game_render_img(frame_buf, { (f32)input->controller.mouse.x,(f32)input->controller.mouse.y }, &game_st->DEBUG_mouse);
}

//TODO(fran): it would be nice to be free of visual studio, so we can do things like live code editing easier, and also simpler for porting




















/*
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
#elif 0 //a little more advanced collision detection
    //day 45, min 37 aprox, starts with collision detection "in p"
    //day 46 just multiplayer support and entities, nothing on collision
    //in the end we are going go to "search in t" (day 47)
    {
        //we test for the walls that are in our range of motion, and then we take the closest point where we hit (in my case with the limited amount of walls we might as well test all, at least to start)
        v2_f32 pos_delta = .5f*dd_word*powf(input->dt_sec,2.f) + player_entity.velocity * input->dt_sec;//NOTE: we'll start just checking a point
        player_entity.velocity += dd_word * input->dt_sec;//TODO: where does this go? //NOTE: Casey put it here, it had it before calculating pos_delta
        //for each wall...
        rc word = { player_entity.pos,player_entity.radius };
        rc wall = { game_st->entities[2].pos , game_st->entities[2].radius };
        //Following the Minkowski Sum now we reduce the word(player) to a point and expand every other object(wall) by the size of the word, so with no need to change any of the code we get for free collision detection for the entire shape of the word
        wall.radius += word.radius;
        v2_f32 min_wall = wall.pos - wall.radius;
        v2_f32 max_wall = wall.pos + wall.radius;

        auto test_wall = [](f32 wallx, f32 oldposx, f32 oldposy, f32 deltax, f32 deltay, f32* t_min, f32 miny, f32 maxy) -> bool {
            if (deltax != 0.f) {
                f32 t_res = (wallx - oldposx) / deltax;
                f32 Y = oldposy + t_res * deltay;
                if (t_res >= 0.f && t_res < *t_min && Y >= miny && Y <= maxy) {
                    f32 epsilon = .001f;
                    *t_min = maximum(.0f, t_res - epsilon); //probably instead of 0 should do epsilon
                    return true;
                }
            }
            return false;
        };
        f32 remaining_t = 1.f;
        for (int attempt = 0; attempt < 4 && remaining_t>0.f; attempt++) {
            f32 closest_t= 1.f; //we limit the time to the full motion that can occur
            v2_f32 wall_normal{ 0,0 };

            if (test_wall(min_wall.x, word.pos.x, word.pos.y, pos_delta.x, pos_delta.y, &closest_t, min_wall.y, max_wall.y)) {
                wall_normal = { -1,0 };
            }
            if (test_wall(max_wall.x, word.pos.x, word.pos.y, pos_delta.x, pos_delta.y, &closest_t, min_wall.y, max_wall.y)) {
                wall_normal = { 1,0 };
            }
            if (test_wall(min_wall.y, word.pos.y, word.pos.x, pos_delta.y, pos_delta.x, &closest_t, min_wall.x, max_wall.x)) {
                wall_normal = { 0,-1 };
            }
            if (test_wall(max_wall.y, word.pos.y, word.pos.x, pos_delta.y, pos_delta.x, &closest_t, min_wall.x, max_wall.x)) {
                wall_normal = { 0,1 };
            }

            player_entity.pos += pos_delta * closest_t;


            //current_level->words[0].velocity += dd_word * input->dt_sec;//TODO: where does this go?
            //if (closest_t < 1.f)
            //    current_level->words[0].velocity = { 0,0 }; //NOTE: REMEMBER: the biggest problem with the player getting stuck was because we werent cancelling it's previous velocity after a collision
            player_entity.velocity -= 1 * dot(player_entity.velocity, wall_normal) * wall_normal;//go in line with the wall
            pos_delta -= 1 * dot(pos_delta, wall_normal) * wall_normal;
            remaining_t -= closest_t*remaining_t; //TODO(fran): I dont think I understand this
        }
    }
#endif
*/