#include "space_typers.h"

//REMAINDERS:
//·optimization fp:fast
//·optimization (google) intel intrinsics guide


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

//bool game_check_collision_point_to_rc(v2_f32 p, rc r) {
//    return p.x > (r.center.x-r.radius.x)&& p.x < (r.center.x + r.radius.x) && p.y >(r.center.y - r.radius.y) && p.y < (r.center.y + r.radius.y);
//}

img DEBUG_load_png(const char* filename) {
    img res{0};
    auto [img_mem, img_sz] = platform_read_entire_file(filename);
    if (img_mem) {
        int width, height, channels;
        u8* bytes = stbi_load_from_memory((u8*)img_mem, img_sz, &width, &height, &channels, 0); //NOTE: this interface always assumes 8 bits per component
        //TODO(fran): the memory for the image should be taken from the arena
        platform_free_file_memory(img_mem);
        if (bytes) {
            game_assert(channels == 4);
            res.mem = bytes;
            res.width = width;
            res.height = height;
            res.pitch = width * IMG_BYTES_PER_PIXEL;
            //res.channels = channels;
            //res.bytes_per_channel = 1; //NOTE: this interface always assumes 8 bits per component

            //NOTE: for premultiplied alpha to work we need to convert all our images to that "format", therefore this changes too

            u8* img_row = (u8*)res.mem; //INFO: R and B values need to be swapped
            for (int y = 0; y < res.height; y++) {
                u32* img_pixel = (u32*)img_row;
                for (int x = 0; x < res.width; x++) {

                    f32 r = (f32)((*img_pixel & 0x000000FF)>>0);
                    f32 g = (f32)((*img_pixel & 0x0000FF00)>>8);
                    f32 b = (f32)((*img_pixel & 0x00FF0000)>>16);
                    f32 a = (f32)((*img_pixel & 0xFF000000)>>24);//TODO(fran): I can just do >>24, check that it puts 0 in the bits left behind and not 1 (when most signif bit is 1)
                    f32 a_n = a / 255.f;

                    r = r * a_n; //premultiplying
                    g = g * a_n; //premultiplying
                    b = b * a_n; //premultiplying

                    //AARRGGBB
                    *img_pixel++ = round_f32_to_u32(a) << 24 | round_f32_to_u32(r) << 16 | round_f32_to_u32(g) << 8 | round_f32_to_u32(b) << 0;
                }
                img_row += res.pitch; 
            }
            //TODO(fran): get align x and y

        }
    }
    return res;
}

void DEBUG_unload_png(img* image) {
    stbi_image_free(image->mem); //TODO(fran): use the memory arena
    //TODO(fran): zero the other members?
}

void game_render_img_ignore_transparency(img* buf, v2_f32 pos, img* image) {

    v2_i32 min = { round_f32_to_i32(pos.x), round_f32_to_i32(pos.y) };
    v2_i32 max = { round_f32_to_i32(pos.x + image->width), round_f32_to_i32(pos.y + image->height) };

    int offset_x = 0;
    int offset_y = 0;

    if (min.x < 0) { offset_x = -min.x;  min.x = 0; }
    if (min.y < 0) { offset_y = -min.y; min.y = 0; }
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;


    u8* row = (u8*)buf->mem + min.y * buf->pitch + min.x * IMG_BYTES_PER_PIXEL;
    u8* img_row = (u8*)image->mem + offset_x * IMG_BYTES_PER_PIXEL + offset_y * image->pitch;

    for (int y = min.y; y < max.y; y++) {
        u32* pixel = (u32*)row;
        u32* img_pixel = (u32*)img_row;
        for (int x = min.x; x < max.x; x++) {
            //AARRGGBB
            *pixel++ = *img_pixel++;
        }
        row += buf->pitch;
        img_row += image->pitch; //NOTE: now I starting seeing the benefits of using pitch, very easy to change rows when you only have to display parts of the img
    }

}

void game_render_img(img* buf, v2_f32 pos, img* image,f32 dimming=1.f) { //NOTE: interesting idea: the thing that we read from and the thing that we write to are handled symmetrically (img-img)

    game_assert(dimming >= 0.f && dimming <= 1.f);

    v2_i32 min = { round_f32_to_i32(pos.x), round_f32_to_i32(pos.y) };
    v2_i32 max = { round_f32_to_i32(pos.x+ image->width), round_f32_to_i32(pos.y+ image->height) };

    int offset_x=0;
    int offset_y=0;

    if (min.x < 0) { offset_x = -min.x ;  min.x = 0; }
    if (min.y < 0) { offset_y = -min.y; min.y = 0; }
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;

    u8* row = (u8*)buf->mem + min.y * buf->pitch + min.x * IMG_BYTES_PER_PIXEL;
    u8* img_row = (u8*)image->mem + offset_x* IMG_BYTES_PER_PIXEL + offset_y*image->pitch;
    
    for (int y = min.y; y < max.y; y++) {
        u32* pixel = (u32*)row;
        u32* img_pixel = (u32*)img_row;
        for (int x = min.x; x < max.x; x++) {
            //AARRGGBB

            //very slow linear blend
            f32 s_a = (f32)((*img_pixel >> 24) & 0xFF); //source
            f32 s_r = dimming * (f32)((*img_pixel >> 16) & 0xFF); //NOTE: dimming also has to premultiply
            f32 s_g = dimming * (f32)((*img_pixel >> 8) & 0xFF);
            f32 s_b = dimming * (f32)((*img_pixel >> 0) & 0xFF);

            f32 r_s_a = (s_a / 255.f)* dimming; //TODO(fran): add extra alpha reduction for every pixel CAlpha (handmade)

            f32 d_a = (f32)((*pixel >> 24) & 0xFF); //dest
            f32 d_r = (f32)((*pixel >> 16) & 0xFF); 
            f32 d_g = (f32)((*pixel >> 8) & 0xFF);
            f32 d_b = (f32)((*pixel >> 0) & 0xFF);

            f32 r_d_a = d_a / 255.f;

            //REMEMBER: handmade day 83, finally understanding what premultiplied alpha is and what it is used for

            //TODO(fran): look at sean barrett's article on premultiplied alpha, remember: "non premultiplied alpha does not distribute over linear interpolation"

            f32 rem_r_s_a = (1.f - r_s_a); //remaining alpha
            f32 a = 255.f*(r_s_a + r_d_a - r_s_a*r_d_a ); //final premultiplication step (handmade day 83 min 1:23:00)
            f32 r = rem_r_s_a * d_r + s_r;//NOTE: before premultiplied alpha we did f32 r = rem_r_s_a * d_r + r_s_a * s_r; now we dont need to multiply the source, it already comes premultiplied
            f32 g = rem_r_s_a * d_g + s_g;
            f32 b = rem_r_s_a * d_b + s_b;

            *pixel = round_f32_to_i32(a)<<24 | round_f32_to_i32(r)<<16 | round_f32_to_i32(g)<<8 | round_f32_to_i32(b)<<0; //TODO(fran): should use round_f32_to_u32?

            pixel++;
            img_pixel++;
        }
        row += buf->pitch;
        img_row += image->pitch; //NOTE: now Im starting to see the benefits of using pitch, very easy to change rows when you only have to display parts of the img
    }

}


struct min_max_v2_f32 { v2_f32 min, max; }; //NOTE: game_coords means meters //TODO(fran): change to rc2
min_max_v2_f32 transform_to_screen_coords(rc2 game_coords, f32 meters_to_pixels, v2_f32 camera_pixels, v2_f32 lower_left_pixels) {
    v2_f32 min_pixels = { (game_coords.center.x - game_coords.radius.x) * meters_to_pixels, (game_coords.center.y + game_coords.radius.y) * meters_to_pixels };

    v2_f32 min_pixels_camera = min_pixels - camera_pixels;

    //INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
    v2_f32 min_pixels_camera_screen = { lower_left_pixels.x + min_pixels_camera.x , lower_left_pixels.y - min_pixels_camera.y };

    v2_f32 max_pixels = { (game_coords.center.x + game_coords.radius.x) * meters_to_pixels, (game_coords.center.y - game_coords.radius.y) * meters_to_pixels };

    v2_f32 max_pixels_camera = max_pixels - camera_pixels;

    v2_f32 max_pixels_camera_screen = { lower_left_pixels.x + max_pixels_camera.x , lower_left_pixels.y - max_pixels_camera.y };

    return { min_pixels_camera_screen , max_pixels_camera_screen };
}

void render_char(img* buf, min_max_v2_f32 min_max, u8* bmp, int width, int height) { //TODO(fran): premultiplied alpha? and generalize img to img operation
    //NOTE: stb creates top down img with 1 byte per pixel
    v2_i32 min = { round_f32_to_i32(min_max.min.x), round_f32_to_i32(min_max.min.y) };
    v2_i32 max = { round_f32_to_i32(min_max.max.x), round_f32_to_i32(min_max.max.y) };

    int offset_x = 0;
    int offset_y = 0;

    if (min.x < 0) { offset_x = -min.x;  min.x = 0; }
    if (min.y < 0) { offset_y = -min.y; min.y = 0; }
    if (max.x > buf->width)max.x = buf->width;
    if (max.y > buf->height)max.y = buf->height;

    int img_pitch = width;

    u8* row = (u8*)buf->mem + min.y * buf->pitch + min.x * IMG_BYTES_PER_PIXEL;
    u8* img_row = bmp + offset_x + offset_y * img_pitch;

    f32 color_r = 255.f;
    f32 color_g = 255.f;
    f32 color_b = 255.f;

    for (int y = min.y; y < max.y; y++) {
        u32* pixel = (u32*)row;
        u8* img_pixel = img_row;
        for (int x = min.x; x < max.x; x++) {
            //AARRGGBB

            //very slow linear blend
            f32 s_a = (f32)(*img_pixel) / 255.f; //source
            f32 s_r = color_r;
            f32 s_g = color_g;
            f32 s_b = color_b;

            f32 d_r = (f32)((*pixel >> 16) & 0xFF); //dest
            f32 d_g = (f32)((*pixel >> 8) & 0xFF);
            f32 d_b = (f32)((*pixel >> 0) & 0xFF);

            f32 r = (1.f - s_a) * d_r + s_a * s_r; //TODO(fran): what if the dest had an alpha different from 255?
            f32 g = (1.f - s_a) * d_g + s_a * s_g;
            f32 b = (1.f - s_a) * d_b + s_a * s_b;

            *pixel = 0xFF << 24 | round_f32_to_i32(r) << 16 | round_f32_to_i32(g) << 8 | round_f32_to_i32(b) << 0;

            pixel++;
            img_pixel++;
        }
        row += buf->pitch;
        img_row += img_pitch; 
    }
}

void game_render_rectangle(img* buf, min_max_v2_f32 min_max, argb_f32 colorf) {
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

    u8* row = (u8*)buf->mem + min.y * buf->pitch + min.x * IMG_BYTES_PER_PIXEL;
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
    game_assert(game_st->entity_count+1 < arr_count(game_st->entities));
    game_st->entities[game_st->entity_count] = *new_entity; //TODO(fran): im pretty sure this cant be a straight copy no more, now we have the collision array
    game_st->entity_count++;
}

void game_remove_entity(game_state* gs, u32 index) {
    assert(index < gs->entity_count);
    if (index != gs->entity_count - 1) {
        gs->entities[index] = gs->entities[gs->entity_count - 1];
    }
    gs->entity_count--;
}

void game_clear_entities(game_state* game_st) {
    game_st->entity_count = 1;//TODO(fran): once again, 1 or 0?
    game_assert(game_st->entities[0].type == entity_null);
}

bool test_wall(f32 wallx, f32 oldposx, f32 oldposy, f32 deltax, f32 deltay, f32* t_min, f32 miny, f32 maxy){ //REMEMBER: test_wall has the "hidden" rule that it checks the current t_min and only changes it if the new one is smaller
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

game_entity* get_entity(game_state* gs, u32 index) { 
    return &gs->entities[index];
}

//INFO: allows for specifying specific This entity vs This entity collision decisions, not just by entity type
void add_collision_rule(game_state* gs, game_entity* A, game_entity* B, bool can_collide) { //TODO(fran): change game_entity* to u32 indexes
    //TODO: collapse this with should_collide
    if (A > B) { //enforce order to simplify hash table access //TODO(fran): again probably straight checking the pointers is no good
        game_entity* temp = A;
        A = B;
        B = temp;
    }

    //TODO(fran): better hash function
    pairwise_collision_rule* found = 0;
    u32 hash_bucket = (u32)A & (arr_count(gs->collision_rule_hash) - 1);
    for (pairwise_collision_rule* rule = gs->collision_rule_hash[hash_bucket]; rule; rule = rule->next_in_hash) {
        if (rule->a == A && rule->b == B) {
            found = rule;
            break;
        }
    }
    if (!found) {
        found = gs->first_free_collision_rule;
        if (found) {
            gs->first_free_collision_rule = gs->first_free_collision_rule->next_in_hash;
        }
        else {
            found = push_type(&gs->memory_arena, pairwise_collision_rule);
        }
        found->next_in_hash = gs->collision_rule_hash[hash_bucket];
        gs->collision_rule_hash[hash_bucket] = found;
    }
    if (found) {
        found->a = A;
        found->b = B;
        found->can_collide = can_collide;
    }
}

bool can_overlap(game_state* gs, game_entity* mover, game_entity* region) {
    bool res = false;
    if (mover != region) {
        if (region->type == entity_word_spawner) {
            res = true;
        }
    }
    return res;
}

void handle_overlap(game_state* gs, game_entity* mover, game_entity* region) {
    //NOTE: im following handmade hero's idea for overlapping to learn new things, but probably this system is unnecessary for this game and should TODO: change it (I liked the was overlapping idea, checking collision begin, end, and already overlapping)
}

bool can_collide(game_state* gs, game_entity* A, game_entity* B) {
    bool res=false;
    if (A != B) {
        if (A > B) { //enforce order to simplify hash table access //TODO(fran): again probably straight checking the pointers is no good
            game_entity* temp = A;
            A = B;
            B = temp;
        }

        if (is_set(A, entity_flag_alive) && is_set(B, entity_flag_alive)) res = true; //TODO(fran): property based logic goes here

        if (A->type == entity_word_spawner || B->type == entity_word_spawner) res = false; //TODO(fran): if I dont do this words cant get out of the spawner cause they get a tmin of 0, why doesnt this happen always someones collides with something?
        //is_set(possible_collider, entity_flag_collides) && is_set(possible_collider, entity_flag_solid) && possible_collider != entity

        //TODO(fran): better hash function
        u32 hash_bucket = (u32)A & (arr_count(gs->collision_rule_hash) - 1);//TODO(fran): add u32 storage_index variable to game_entity struct, casting to u32 a pointer doesnt look too nice
        for (pairwise_collision_rule* rule = gs->collision_rule_hash[hash_bucket]; rule; rule = rule->next_in_hash) {
            if (rule->a == A && rule->b == B) {
                res = rule->can_collide; //the rule overrides any other condition
                break;
            }
        }
    }
    return res;
}

bool handle_collision(game_entity* A, game_entity* B) {
    bool stops_on_collision = false;// is_set(entity, entity_flag_collides);

    if (A->type == entity_word || B->type == entity_word || A->type==entity_word_spawner || B->type == entity_word_spawner)
        stops_on_collision = false;
    else 
        stops_on_collision = true;

    if (A->type > B->type) { //enforce order to reduce collision handling routines in almost half
        game_entity* temp = A;
        A = B;
        B = temp;
    }

    if (A->type == entity_word && B->type == entity_wall) {
        clear_flags(A, entity_flag_alive); //TODO(fran): once a word hits a wall we probably want to add a collision rule for avoiding further collisions between those two (right now it doesnt matter cause we destroy the word at the end of the frame but later there'll probably be some animation, like a fade, of the word before being destroyed and that would cause multiple collisions thus, for example reducing the life of the player more than once)
    }

    //add_collision_rule(gs,entity, hit_entity, false);
 
    return stops_on_collision;
}

bool entities_overlap(game_entity* entity, game_entity* test_entity) {
    bool res = false;
    for (u32 entity_area_idx = 0; !res && entity_area_idx < entity->collision.area_count; entity_area_idx++) {
        game_entity_collision_area* entity_area = &entity->collision.areas[entity_area_idx];
        rc2 entity_collider = rc_center_radius(entity->pos + entity_area->offset, entity_area->radius);
        for (u32 test_collider_area_idx = 0; !res && test_collider_area_idx < test_entity->collision.area_count; test_collider_area_idx++) {//TODO(fran): optimization: maybe break is faster than always checking !res in both loops
            game_entity_collision_area* test_area = &test_entity->collision.areas[test_collider_area_idx];
            rc2 test_collider = rc_center_radius(test_entity->pos + test_area->offset, test_area->radius);
            res = rcs_intersect(entity_collider, test_collider);
        }
    }
    return res;
}

void move_entity(game_entity* entity, v2_f32 acceleration, game_state* gs, game_input* input) { //NOTE: im starting to feel the annoyance of const, everything that is used inside here (other functions for example) would have to get const'd as well, so out it went
    //rc2 e = rc_center_radius(entity->pos, entity->radius);
    
    v2_f32 pos_delta = .5f * acceleration * powf(input->dt_sec, 2.f) + entity->velocity * input->dt_sec;//NOTE: we'll start just checking a point
    entity->velocity += acceleration * input->dt_sec;//TODO: where does this go? //NOTE: Casey put it here, it had it before calculating pos_delta
    
    //NOTE: day 67, min ~30 presents ideas on collision detection and the ways of operating on the entities that have responses/specific behaviours to collisions 
    //NOTE: day 80 interesting handling of "traversable" entities (overlap)

    //if (is_set(entity,entity_flag_collides)) { //NOTE: everybody checks collisions, for now
        for (int attempt = 0; attempt < 4; attempt++) {
            f32 closest_t = 1.f; //limits the time to the full motion that can occur
            v2_f32 wall_normal{ 0,0 };
            game_entity* hit_entity = 0;
            v2_f32 desired_position = entity->pos + pos_delta;
            for (u32 entity_index = 1; entity_index < gs->entity_count; entity_index++)//TODO(fran): what to do, always start from 1?
            {
                game_entity* possible_collider = get_entity(gs,entity_index);//TODO(fran): GetEntityIndex function
                //TODO(fran): re-integrate the "solid" flag or create a new "overlappable" flag
                if (can_collide(gs,entity,possible_collider)) { //TODO(fran): add? can_collide || can_overlap &&  entities_overlap
                    for (u32 entity_area_idx = 0; entity_area_idx < entity->collision.area_count; entity_area_idx++) {
                        game_entity_collision_area* entity_area = &entity->collision.areas[entity_area_idx];
                        rc2 e = rc_center_radius(entity->pos+entity_area->offset, entity_area->radius);
                        for (u32 possible_collider_area_idx = 0; possible_collider_area_idx < possible_collider->collision.area_count; possible_collider_area_idx++) {
                            game_entity_collision_area* possible_collider_area = &possible_collider->collision.areas[possible_collider_area_idx];
                            rc2 collider = rc_center_radius(possible_collider->pos+ possible_collider_area->offset, possible_collider_area->radius);
                            //Following the Minkowski Sum now we reduce the entity to a point and expand every other object by the size of the entity, so with no need to change any of the code we get for free collision detection for the entire shape of the entity
                            collider.radius += e.radius;
                            v2_f32 min_collider = collider.get_min();
                            v2_f32 max_collider = collider.get_max();


                            //TODO: mirar dia 52 min 9:47, handmade hero NO usa e.pos acá sino "Rel" que es igual a e.pos-collider.pos
                            if (test_wall(min_collider.x, e.center.x, e.center.y, pos_delta.x, pos_delta.y, &closest_t, min_collider.y, max_collider.y)) {
                                wall_normal = { -1,0 };
                                hit_entity = possible_collider;
                            }
                            if (test_wall(max_collider.x, e.center.x, e.center.y, pos_delta.x, pos_delta.y, &closest_t, min_collider.y, max_collider.y)) {
                                wall_normal = { 1,0 };
                                hit_entity = possible_collider;
                            }
                            if (test_wall(min_collider.y, e.center.y, e.center.x, pos_delta.y, pos_delta.x, &closest_t, min_collider.x, max_collider.x)) {
                                wall_normal = { 0,-1 };
                                hit_entity = possible_collider;
                            }
                            if (test_wall(max_collider.y, e.center.y, e.center.x, pos_delta.y, pos_delta.x, &closest_t, min_collider.x, max_collider.x)) {
                                wall_normal = { 0,1 };
                                hit_entity = possible_collider;
                            }
                        }
                    }
                }
            }
            entity->pos += pos_delta * closest_t;
            if (hit_entity) {
                pos_delta = desired_position - entity->pos;

                //NOTE: handmade hero day 68, interesting concept, resolving an n^2 problem (double dispatch) with an n^2 solution
                
                bool stops_on_collision = handle_collision(entity, hit_entity);

                if (stops_on_collision) {
                    entity->velocity -= 1 * dot(entity->velocity, wall_normal) * wall_normal;//go in line with the wall
                    pos_delta -= 1 * dot(pos_delta, wall_normal) * wall_normal;
                } //TODO(fran): need the collision table to avoid re-collisions
                else {
                    add_collision_rule(gs, entity, hit_entity, false); //we avoid that re-collisions decrease our closest_t //TODO(fran): problem now is when to remove this rule //NOTE: casey took it out of here
                }
     
            }
            else {
                break;
            }
            //if (closest_t < 1.f) current_level->words[0].velocity = { 0,0 }; //REMEMBER: the biggest problem with the player getting stuck was because we werent cancelling it's previous velocity after a collision
        }
    //}
    //else {
    //    entity->pos += pos_delta;
    //}

        {//Handle events based on overlapping
            for (u32 test_entity_index = 1; test_entity_index < gs->entity_count; test_entity_index++) {
                game_entity* test_entity = get_entity(gs, test_entity_index);
                if (can_overlap(gs, entity, test_entity) && entities_overlap(entity, test_entity)) {
                        handle_overlap(gs, entity, test_entity);
                }
            }
        }

}

void game_initialize_entities(game_state* gs, game_level_map lvl) {
    game_clear_entities(gs);

    for (u32 i = 0; i < lvl.entity_count; i++) {
        game_add_entity(gs, &lvl.entities[i]);
    }
}

void clear_collision_rules_for(game_state* gs, game_entity* e) {
    //TODO(fran): make better data structure that allows for removing of collision rules without searching the entire table
    for (u32 hash_bucket = 0; hash_bucket < arr_count(gs->collision_rule_hash); hash_bucket++) {
        for (pairwise_collision_rule** rule = &gs->collision_rule_hash[hash_bucket]; *rule;) {
            if ((*rule)->a == e || (*rule)->b == e) { 
                pairwise_collision_rule* removed = *rule;
                *rule = (*rule)->next_in_hash;
                removed->next_in_hash = gs->first_free_collision_rule;
                gs->first_free_collision_rule = removed;
            }
            else {
                rule = &(*rule)->next_in_hash;
            }
        }
    }
}

game_entity_collision_area_group make_null_collision_box() {
    game_entity_collision_area_group g;
    g.areas = 0;
    g.area_count = 0;
    g.total_area = { 0 };
    return g;
}

game_entity_collision_area_group make_simple_collision_box(game_memory_arena* arena, v2_f32 offset, v2_f32 radius) {
    game_entity_collision_area_group g;
    g.area_count = 1;
    g.total_area.offset = offset;
    g.total_area.radius = radius;
    g.areas = push_arr(arena, game_entity_collision_area, g.area_count);
    g.areas[0] = g.total_area;
    //g.areas = push_mem(arena, game_entity_collision_area); //TODO
    //g.areas[0].offset = offset;
    //g.areas[0].radius = radius;
    return g;
}

//NOTE: radius is used to create the collision area, TODO(fran): I dont think I want to create the collision areas until the level is loaded, so maybe better is to store the radius (and offset) in an array, on the other hand... that's almost the same size as the collision area itself, hmm
game_entity create_wall(game_memory_arena* arena, v2_f32 pos,v2_f32 radius,argb_f32 color) {//TODO(fran): v4 with both xyzw and rgba access
    game_entity wall;
    wall.acceleration = { 0,0 };
    wall.flags = entity_flag_collides | entity_flag_alive | entity_flag_solid;
    wall.color = color;
    wall.pos = pos;
    //wall.radius = radius;
    wall.type = entity_wall;
    wall.velocity = { 0,0 };
    wall.collision = make_simple_collision_box(arena, { 0,0 }, radius);
    return wall;//std::move, I hope the compiler is intelligent enough
}

game_entity create_player(game_memory_arena* arena, v2_f32 pos, v2_f32 radius, argb_f32 color) {
    game_entity player;
    player.flags = entity_flag_collides | entity_flag_alive | entity_flag_solid;
    player.color = color;
    player.pos = pos;
    //player.radius = radius;
    player.velocity = { 0,0 };
    player.type = entity_player;
    player.collision = make_simple_collision_box(arena, { 0,0 }, radius);
    return player;
}

game_entity create_word_spawner(game_memory_arena* arena, v2_f32 pos, v2_f32 radius) {
    game_entity word_spawner;
    word_spawner.acceleration = { 0,0 };
    word_spawner.accumulated_time_sec = 0;
    word_spawner.flags = entity_flag_alive;
    word_spawner.color = { 0, 0, 0.5f, 0 };
    word_spawner.pos = pos;
    //word_spawner.radius = radius;
    word_spawner.time_till_next_word_sec = 5.f;
    word_spawner.type = entity_word_spawner;
    word_spawner.velocity = { 0,0 };
    //TODO(fran): specify: word output direction
    word_spawner.collision = make_simple_collision_box(arena, { 0,0 }, radius);
    return word_spawner;
}

game_entity create_word(game_memory_arena* arena, v2_f32 pos, v2_f32 radius, v2_f32 velocity, argb_f32 color) {
    //TODO(fran): radius should be determined by the lenght of the word it contains
    game_entity word;
    word.flags = entity_flag_collides | entity_flag_alive;
    word.color = color;
    word.pos = pos;
    //word.radius = radius;
    word.velocity = velocity;
    word.type = entity_word;
    word.acceleration = { 0,0 };
    word.collision = make_simple_collision_box(arena, { 0,0 }, radius);
    return word;
}

#define zero_struct(instance) zero_mem(&(instance),sizeof(instance))
void zero_mem(void* ptr, u32 sz) {
    //TODO(fran): performance
    u8* bytes = (u8*)ptr;
    while (sz--)
        *bytes++ = 0;
}

img make_empty_img(game_memory_arena* arena,u32 width, u32 height) {//TODO(fran): should allow for negative sizes?
    img res;
    res.width = width;
    res.height = height;
    res.pitch = width * IMG_BYTES_PER_PIXEL;
    res.mem = _push_mem(arena, width * height * IMG_BYTES_PER_PIXEL);
    zero_mem(res.mem, width * height * IMG_BYTES_PER_PIXEL);
    return res;
}

void game_update_and_render(game_memory* memory, game_framebuffer* frame_buf, game_input* input) {
    
    //BIG TODO(fran): game_add_entity can no longer copy the game_entity, that would mean it uses the collision areas stored in the saved entity (it's fine for now, until we need to apply some transformation to collision areas)
    
    game_assert(sizeof(game_state) <= memory->permanent_storage_sz);
    game_state* gs = (game_state*)memory->permanent_storage;
    
    //INFO IDEA: I see the game, we load stages from the filesystem, each stage can contain stages or levels inside, we show them in a nice rounded squares type menu, you can click to enter any stage/level, then you are sent to the real level

    //NOTE:since our worlds are going to be very small we wont need to do virtualized space like Handmade Hero (day 33), but the idea is there if we decide to reuse the engine for something bigger

    //IDEA: pixel art assets so we can create them fast, Im not sure how well they'll look next to the text for the words, maybe some font can fix that

    //IDEA NOTE: text autocomplete makes you worse at typing, we want to train people to be able to write everything themselves

    //NOTE IDEA: I see an aesthetic with oranges and white, warm colors, reds

    //IDEA: different visible walls for the words to collide against, so it creates sort of a priority system where some words must be typed first cause they are going to collide with a wall that is much closer than the one on the other side of the screen

    if (!memory->is_initialized) {
        //game_st->hz = 256;
        game_entity null_entity{ 0 };
        null_entity.type = entity_null; //NOTE: just in case
        game_add_entity(gs, &null_entity); //NOTE: entity index 0 is reserved as NULL

        gs->word_height_meters = 1.5f;//TODO(fran): maybe this would be better placed inside world
        gs->word_height_pixels = 60;

        gs->lower_left_pixels = { 0.f,(f32)frame_buf->height };

        gs->word_meters_to_pixels = (f32)gs->word_height_pixels / gs->word_height_meters; //Transform from meters to pixels

        gs->word_pixels_to_meters = 1 / gs->word_meters_to_pixels;

        initialize_arena(&gs->memory_arena, (u8*)memory->permanent_storage + sizeof(game_state), memory->permanent_storage_sz - sizeof(game_state));

        gs->world.stages = push_type(&gs->memory_arena,game_stage);
        gs->world.stage_count = 1;
        gs->world.current_stage = 0;

        gs->world.stages[0].lvls = push_arr(&gs->memory_arena, game_level_map, 2);
        gs->world.stages[0].level_count = 2; //TODO(fran): can we use sizeof_arr ?? I doubt it
        gs->world.stages[0].current_lvl = 0;

        game_level_map& level1 = gs->world.stages[0].lvls[0];
        game_level_map& level2 = gs->world.stages[0].lvls[1];

#define LVL1_ENTITY_COUNT 3
        level1.entities = push_arr(&gs->memory_arena, game_entity, LVL1_ENTITY_COUNT);
        level1.entity_count = LVL1_ENTITY_COUNT;

        level2.entities = push_arr(&gs->memory_arena, game_entity, LVL1_ENTITY_COUNT);
        level2.entity_count = LVL1_ENTITY_COUNT;
        {
            game_entity level1_wall = create_wall(&gs->memory_arena, v2_f32{ 8.f , 9.5f }*gs->word_height_meters, v2_f32{ .5f , 4.5f }*gs->word_height_meters, { 0.f,1.f,0.f,0.f });

            game_entity level1_player = create_player(&gs->memory_arena,v2_f32{ 11.f ,5.f }*gs->word_height_meters, v2_f32{ .5f , .5f }*gs->word_height_meters, { .0f,.0f,.0f,1.0f });

            game_entity level1_word_spawner= create_word_spawner(&gs->memory_arena,v2_f32{ 19.5f, 10.5f } *gs->word_height_meters, v2_f32{ .5f , 4.f } *gs->word_height_meters);

            level1.entities[0] = level1_wall;
            level1.entities[1] = level1_player;
            level1.entities[2] = level1_word_spawner;
        }
        {
            game_entity level2_wall = create_wall(&gs->memory_arena,v2_f32{ 8.5f , 7.5f }*gs->word_height_meters, v2_f32{ .5f , 4.5f }*gs->word_height_meters, { 0.f,1.f,0.f,0.f });

            game_entity level2_player = create_player(&gs->memory_arena,v2_f32{ 11.f ,5.f }*gs->word_height_meters, v2_f32{ .75f , .75f }*gs->word_height_meters, { .0f,.0f,.0f,1.0f });

            game_entity level2_word_spawner = create_word_spawner(&gs->memory_arena,v2_f32{ 16.5f , 10.5f }*gs->word_height_meters, v2_f32{ .5f , .5f }*gs->word_height_meters);

            level2.entities[0] = level2_wall;
            level2.entities[1] = level2_player;
            level2.entities[2] = level2_word_spawner;
        }
        
        gs->DEBUG_background = DEBUG_load_png("assets/img/stars.png"); //TODO(fran): release mem DEBUG_unload_png();
        //gs->DEBUG_background.align = { 20,20 };
        gs->DEBUG_menu = DEBUG_load_png("assets/img/down.png"); //TODO(fran): release mem DEBUG_unload_png();
        //gs->DEBUG_menu.align = { 20,20 };
        gs->DEBUG_mouse = DEBUG_load_png("assets/img/mouse.png"); //TODO(fran): release mem DEBUG_unload_png();

        gs->camera = { 0 , 0 }; //TODO(fran): place camera in the world, to simplify first we fix it to the middle of the screen

        game_initialize_entities(gs, gs->world.stages[0].lvls[gs->world.stages[0].current_lvl]);

        memory->is_initialized = true; //TODO(fran): should the platform layer do this?
    }

    //TODO(fran):objects may need z position to resolve conflicts with render overlapping

    //REMEMBER: you get the input for the previous frame
    
    if (input->controller.back.ended_down) {
        gs->world.stages[0].current_lvl++;
        gs->world.stages[0].current_lvl %= gs->world.stages[0].level_count;

        game_initialize_entities(gs, gs->world.stages[0].lvls[gs->world.stages[0].current_lvl]);
    }

    game_assert(gs->entities[0].type == entity_null);

    //IDEA: what if the platform layer gave you scaling information, so you set up your render like you want and then the only thing that needs to change is scaling

    //Update Loop
    for (u32 entity_index = 1; entity_index < gs->entity_count; entity_index++) { //NOTE: remember to start from 1
        switch (gs->entities[entity_index].type) {
        case entity_player: 
        {
            game_entity& player_entity = gs->entities[entity_index];
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
            f32 constant_accel = 20.f * gs->word_height_meters; //m/s^2
            dd_word *= constant_accel;
            //TODO(fran): ordinary differential eqs
            dd_word += -3.5f * player_entity.velocity; //basic "drag" //TODO(fran): understand why this doesnt completely eat the acceleration value

            move_entity(&player_entity, dd_word, gs, input); //TODO(fran): for each entity that moves

            v2_f32 screen_offset = { (f32)frame_buf->width / 2,(f32)frame_buf->height / 2 };
            gs->camera = player_entity.pos * gs->word_meters_to_pixels - screen_offset; //TODO(fran): nicer camera update, lerp //TODO(fran): where to update the camera? //TODO(fran): camera position should be on the center, and at the time of drawing account for width and height from there
        } break;
        case entity_wall: break;
        case entity_word: 
        {
            move_entity(&gs->entities[entity_index], {0,0}, gs, input);
            //move_word(&gs->entities[entity_index],gs,input);
            //TODO(fran): move the word
        } break;
        case entity_word_spawner:
        {
            game_entity& spawner_entity = gs->entities[entity_index];
            spawner_entity.accumulated_time_sec += input->dt_sec;
            if (spawner_entity.accumulated_time_sec > spawner_entity.time_till_next_word_sec) {
                spawner_entity.accumulated_time_sec = 0;
                v2_f32 pos = { lerp(spawner_entity.pos.x + spawner_entity.collision.total_area.offset.x - spawner_entity.collision.total_area.radius.x,spawner_entity.pos.x + spawner_entity.collision.total_area.offset.x + spawner_entity.collision.total_area.radius.x, random_unilateral()),lerp(spawner_entity.pos.y + spawner_entity.collision.total_area.offset.y - spawner_entity.collision.total_area.radius.y,spawner_entity.pos.y + spawner_entity.collision.total_area.offset.y + spawner_entity.collision.total_area.radius.y, random_unilateral()) }; //TODO: probably having a rect struct is better, I can more easily get things like the min point //TODO(fran): pick one of the collision areas and spawn from there, or maybe better to just say we use only one for this guy, since it also needs to generate the direction of the word, and then we'd need to store different directions
                
                game_entity word = create_word(&gs->memory_arena,pos, v2_f32{ .5f , .5f  }*gs->word_height_meters, v2_f32{ -1,0 }*(5 * gs->word_height_meters)* random_unilateral(), { random_unilateral(),random_unilateral() ,random_unilateral() ,random_unilateral() });
                
                game_add_entity(gs, &word);

            }
        } break;
        case entity_null: game_assert(0);
        default: game_assert(0);
        }
    }

    //game_render_rectangle(frame_buf, { (f32)input->controller.mouse.x,(f32)input->controller.mouse.y }, {(f32)input->controller.mouse.x + 16, (f32)input->controller.mouse.y+16 }, player_color);

    img framebuffer;
    framebuffer.height = frame_buf->height;
    framebuffer.width = frame_buf->width;
    framebuffer.mem = frame_buf->mem;
    framebuffer.pitch = frame_buf->pitch;

#if _DEBUG
    game_render_rectangle(&framebuffer, { { (f32)0,(f32)0 }, { (f32)framebuffer.width,(f32)framebuffer.height } }, { 0,1.f,0.f,1.f });
#endif

    game_render_img_ignore_transparency(&framebuffer, { (f32)framebuffer.width/2 - gs->DEBUG_background.width / 2,(f32)framebuffer.height/2 - gs->DEBUG_background.height/2 } , &gs->DEBUG_background);
    //NOTE or TODO(fran): stb gives the png in correct orientation by default, I'm not sure whether that's gonna cause problems with our orientation reversing
    game_render_img(&framebuffer, { 0,(f32)framebuffer.height - gs->DEBUG_menu.height }, &gs->DEBUG_menu,.5f); //NOTE: it's pretty interesting that you can actually see information in the pixels with full alpha, the program that generates them does not put rgb to 0 so you can see "hidden" things

    //NOTE: now when we go to render we have to transform from meters, the unit everything in our game is, to pixels, the unit of the screen
    

    //Render Loop, TODO(fran): separate by entity type
    for (u32 i = 1; i < gs->entity_count; i++) {
        const game_entity* e = &gs->entities[i];
        game_render_rectangle( //TODO(fran): change to render_bounding_box (just the borders)
            &framebuffer,
            transform_to_screen_coords({ e->pos + e->collision.total_area.offset , e->collision.total_area.radius }, gs->word_meters_to_pixels, gs->camera, gs->lower_left_pixels),
            e->color); //TODO(fran): iterate over all collision areas and render each one
        switch (e->type) {
        case entity_word:
        {
            stbtt_fontinfo font;
            int width, height; char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
            platform_read_entire_file_res f = platform_read_entire_file("c:/windows/fonts/arial.ttf"); //TODO(fran): fonts folder inside /assets
            stbtt_InitFont(&font, (u8*)f.mem, stbtt_GetFontOffsetForIndex((u8*)f.mem, 0));
            u8* bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, 50.f), alphabet[random_count(arr_count(alphabet)-1)], &width, &height, 0, 0);
            render_char(&framebuffer,
                transform_to_screen_coords(rc_center_radius( e->pos , v2_f32{(f32)width/2.f,(f32)height/2.f}*gs->word_pixels_to_meters), 
                    gs->word_meters_to_pixels, gs->camera, gs->lower_left_pixels), bitmap, width, height);
            platform_free_file_memory(f.mem);
            stbtt_FreeBitmap(bitmap, 0);
        } break;
        }
    }

    game_render_img(&framebuffer, { (f32)input->controller.mouse.x,(f32)input->controller.mouse.y }, &gs->DEBUG_mouse);

    //Deletion Loop //TODO(fran): should deletion happen before render?
    for (u32 i = 1; i < gs->entity_count; i++) {
        if (!is_set(&gs->entities[i], entity_flag_alive)) { //TODO(fran): would a destroy flag be better? so you dont have to set alive on each entity creation
            clear_collision_rules_for(gs, &gs->entities[i]);//NOTE: I dont really know if the collision rule idea will be useful for the simple game im making right now, but it looks like an interesting concept to understand so I'm doing it anyway to learn
            game_remove_entity(gs, i);
        }
    }

}

//TODO(fran): it would be nice to be free of visual studio, so we can do things like live code editing easier, and also simpler for porting

//INFO: handmade day 55 nice example of a simple hashmap

//TODO(fran): see day 56 again

//INFO: handmade day 61, min ~30 "hit table" to differentiate between new collisions and ones that were already overlapping that same object
//INFO: day 62 introduces an interesting concept of "move spec" which abstracts movement a bit, and adds options for different types













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