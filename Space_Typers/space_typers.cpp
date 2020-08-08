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

//bool game_check_collision_point_to_rc(v2 p, rc r) {
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

            //NOTE: for premultiplied alpha to work we need to convert all our images to that "format" when loaded, therefore this changes too

            u8* img_row = (u8*)res.mem; //INFO: R and B values need to be swapped
            for (int y = 0; y < res.height; y++) {
                u32* img_pixel = (u32*)img_row;
                for (int x = 0; x < res.width; x++) {

                    v4 texel = { (f32)((*img_pixel & 0x000000FF) >> 0),
                                 (f32)((*img_pixel & 0x0000FF00) >> 8),
                                 (f32)((*img_pixel & 0x00FF0000) >> 16),
                                 (f32)((*img_pixel & 0xFF000000) >> 24) };//TODO(fran): I can just do >>24, check that it puts 0 in the bits left behind and not 1 (when most signif bit is 1)
#if 1
                    texel = srgb255_to_linear1(texel);
                    
                    texel.rgb *= texel.a;//premultiplying

                    texel = linear1_to_srgb255(texel);
#endif
                    //AARRGGBB
                    *img_pixel++ = round_f32_to_u32(texel.a) << 24 | round_f32_to_u32(texel.r) << 16 | round_f32_to_u32(texel.g) << 8 | round_f32_to_u32(texel.b) << 0;
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

void render_char(img* buf, rc2 rect, u8* bmp, int width, int height) { //TODO(fran): premultiplied alpha? and generalize img to img operation
    //NOTE: stb creates top down img with 1 byte per pixel
    v2_i32 min = { round_f32_to_i32(rect.get_min().x), round_f32_to_i32(rect.get_min().y) };
    v2_i32 max = { round_f32_to_i32(rect.get_max().x), round_f32_to_i32(rect.get_max().y) };

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

void game_add_entity(game_state* game_st, game_entity* new_entity) {
    game_assert(game_st->entity_count+1 < arr_count(game_st->entities));
    game_st->entities[game_st->entity_count] = *new_entity; //TODO(fran): im pretty sure this cant be a straight copy no more, now we have the collision array
    game_st->entity_count++;
}

void game_remove_entity(game_state* gs, u32 index) { //TODO(fran): remove entity for real, clear img and etc
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
            found = push_type(&gs->permanent_arena, pairwise_collision_rule);//TODO(fran): pass arena as param
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

void move_entity(game_entity* entity, v2 acceleration, game_state* gs, game_input* input) { //NOTE: im starting to feel the annoyance of const, everything that is used inside here (other functions for example) would have to get const'd as well, so out it went
    //rc2 e = rc_center_radius(entity->pos, entity->radius);
    
    v2 pos_delta = .5f * acceleration * squared(input->dt_sec) + entity->velocity * input->dt_sec;//NOTE: we'll start just checking a point
    entity->velocity += acceleration * input->dt_sec;//TODO: where does this go? //NOTE: Casey put it here, it had it before calculating pos_delta
    
    //NOTE: day 67, min ~30 presents ideas on collision detection and the ways of operating on the entities that have responses/specific behaviours to collisions 
    //NOTE: day 80 interesting handling of "traversable" entities (overlap)

    //if (is_set(entity,entity_flag_collides)) { //NOTE: everybody checks collisions, for now
        for (int attempt = 0; attempt < 4; attempt++) {
            f32 closest_t = 1.f; //limits the time to the full motion that can occur
            v2 wall_normal{ 0,0 };
            game_entity* hit_entity = 0;
            v2 desired_position = entity->pos + pos_delta;
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
                            v2 min_collider = collider.get_min();
                            v2 max_collider = collider.get_max();


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

game_entity_collision_area_group make_simple_collision_box(game_memory_arena* arena, v2 offset, v2 radius) {
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
game_entity create_wall(game_memory_arena* arena, v2 pos,v2 radius, v4 color) {//TODO(fran): v4 with both xyzw and rgba access
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

game_entity create_player(game_memory_arena* arena, v2 pos, v2 radius, v4 color) {
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

game_entity create_word_spawner(game_memory_arena* arena, v2 pos, v2 radius) {
    game_entity word_spawner;
    word_spawner.acceleration = { 0,0 };
    word_spawner.accumulated_time_sec = 0;
    word_spawner.flags = entity_flag_alive;
    word_spawner.color = { 0, 0.5f, 0, 0 };
    word_spawner.pos = pos;
    //word_spawner.radius = radius;
    word_spawner.time_till_next_word_sec = 5.f;
    word_spawner.type = entity_word_spawner;
    word_spawner.velocity = { 0,0 };
    //TODO(fran): specify: word output direction
    word_spawner.collision = make_simple_collision_box(arena, { 0,0 }, radius);
    return word_spawner;
}

void clear_img(img* image) {
    if(image->mem)
        zero_mem(image->mem, image->width * image->height * IMG_BYTES_PER_PIXEL);
}

img make_empty_img(game_memory_arena* arena, u32 width, u32 height, bool clear_to_zero=true) {//TODO(fran): should allow for negative sizes?
    img res;
    res.width = width;
    res.height = height;
    res.pitch = width * IMG_BYTES_PER_PIXEL;
    res.alignment_px = v2{ 0,0 };
    res.mem = _push_mem(arena, width * height * IMG_BYTES_PER_PIXEL);
    if(clear_to_zero)
        clear_img(&res);
    return res;
}

img make_word_background_image(game_state* gs, game_memory_arena* transient_arena, u32 word_width_px, u32 word_height_px) {
    i32 borders_hor = 1;
    i32 borders_vert = 1;
    i32 img_width = gs->word_corner.width * 2 + gs->word_border.width * borders_hor;
    i32 img_height = gs->word_corner.height * 2 + gs->word_border.width * borders_vert;
    img res = make_empty_img(transient_arena, img_width, img_height); //TODO(fran): pass arena as param
    //TODO(fran): would it be better to render the top and then flip it to draw the bottom?

    //IMPORTANT TODO(fran): see handmade day 85 min 39, would it make sense to store the new images in a transient arena? if so what happens when the arena gets cleared, how do I know from the word's point of view, that it's img is no longer valid? should I instead create some structure (hash table, etc) that provides access to the imgs and is referenced say by the width and height of the img, if that combination is not present then I create it and enter the entry into the hash table, we could use a similar scheme to the one handmade hero plans in that episode, with the lru(least recently used) idea, we can predict, for example a limit of say 30 words at maximum visible in a single moment (note: he made a couple typos and initialized memory with the mem size instead of the pointer)

    //TODO(fran): make this go through the pipeline, create a render_group here

    //draw corners
    i32 offsetx = 0;
    i32 offsety = 0;
    game_render_img(&res, v2{ (f32)offsetx + (f32)gs->word_border.width / 2.f,(f32)offsety + (f32)gs->word_border.height / 2.f }, &gs->word_corner);
    offsetx = gs->word_corner.width + gs->word_border.width * borders_hor;
    offsety = 0;
    game_render_img(&res, v2{ (f32)offsetx + (f32)gs->word_border.width / 2.f,(f32)offsety + (f32)gs->word_border.height / 2.f }, &gs->word_corner);
    offsetx = 0;
    offsety = gs->word_corner.height + gs->word_border.height * borders_vert;
    game_render_img(&res, v2{ (f32)offsetx + (f32)gs->word_border.width / 2.f,(f32)offsety + (f32)gs->word_border.height / 2.f }, &gs->word_corner);
    offsetx = gs->word_corner.width + gs->word_border.width * borders_hor;
    offsety = gs->word_corner.height + gs->word_border.height * borders_vert;
    game_render_img(&res, v2{ (f32)offsetx + (f32)gs->word_border.width / 2.f,(f32)offsety + (f32)gs->word_border.height / 2.f }, & gs->word_corner);

    //draw top border
    offsetx = gs->word_corner.width;
    offsety = 0;
    for (i32 i = 0; i < borders_hor; i++) {
        game_render_img(&res, v2_from_i32(offsetx + gs->word_border.width / 2, offsety+ gs->word_border.height / 2), &gs->word_border);
        offsetx+= gs->word_border.width;
    }

    //draw bottom border
    offsetx = gs->word_corner.width;
    offsety = gs->word_corner.height + gs->word_border.height*borders_vert;
    for (i32 i = 0; i < borders_hor; i++) {
        game_render_img(&res, v2_from_i32(offsetx + gs->word_border.width / 2, offsety + gs->word_border.height / 2), &gs->word_border);
        offsetx += gs->word_border.width;
    }

    //draw left border
    offsetx = 0;
    offsety = gs->word_corner.height;
    for (i32 i = 0; i < borders_hor; i++) {
        game_render_img(&res, v2_from_i32(offsetx + gs->word_border.width / 2, offsety + gs->word_border.height / 2), &gs->word_border);
        offsety += gs->word_corner.height;
    }

    //draw right border
    offsetx = gs->word_corner.width+gs->word_border.width*borders_hor;
    offsety = gs->word_corner.height;
    for (i32 i = 0; i < borders_hor; i++) {
        game_render_img(&res, v2_from_i32(offsetx + gs->word_border.width / 2, offsety + gs->word_border.height / 2), &gs->word_border);
        offsety += gs->word_corner.height;
    }
    return res;
}

game_entity create_word(game_state* gs, game_memory_arena* transient_arena, v2 pos, v2 radius, v2 velocity, v4 color) {
    //TODO(fran): radius should be determined by the lenght of the word it contains
    game_entity word;
    word.flags = entity_flag_collides | entity_flag_alive;
    word.color = color;
    word.pos = pos;
    //word.radius = radius;
    word.velocity = velocity;
    word.type = entity_word;
    word.acceleration = { 0,0 };
    word.collision = make_simple_collision_box(transient_arena, { 0,0 }, radius); //TODO(fran): arena should be passed as separate param

    char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    word.txt[0] = alphabet[random_count(arr_count(alphabet) - 1)];
    //TODO(fran): generate bitmap based on word metrics (we will need font info)
    // we need a square generator that takes a corner, a side, and an inside (rotates the corner to generate the 4 ones)
    word.image = make_word_background_image(gs, transient_arena, 0, 0);

    return word;
}

//i8 bilateral_f32_to_i8(f32 n) {
//    f32 res;
//    if (n >= 0) res = n * 127;
//    else res = n * 128;
//    return (i8)res;
//}
#if 0
img make_sphere_normal_map(game_memory_arena* arena, i32 width, i32 height, f32 Roughness=0.f)
{
    img res = make_empty_img(arena, width, height);
    img* Bitmap = &res;
    f32 InvWidth = 1.0f / (f32)(Bitmap->width - 1);
    f32 InvHeight = 1.0f / (f32)(Bitmap->height - 1);

    u8* Row = (u8*)Bitmap->mem;

    for (i32 Y = 0;
        Y < Bitmap->height;
        ++Y)
    {
        u32* Pixel = (u32*)Row;
        for (i32 X = 0;
            X < Bitmap->width;
            ++X)
        {
            v2 BitmapUV = { InvWidth * (f32)X, InvHeight * (f32)Y };

            f32 Nx = (2.0f * BitmapUV.x - 1.0f);
            f32 Ny = (2.0f * BitmapUV.y - 1.0f);

            f32 RootTerm = 1.0f - Nx * Nx - Ny * Ny;
            v3 Normal = { 0, 0.707106781188f, 0.707106781188f };
            f32 Nz = 0.0f;
            if (RootTerm >= 0.0f)
            {
                Nz = square_root(RootTerm);
                Normal = v3{ Nx, Ny, Nz };
            }

            v4 Color = { 255.0f * (0.5f * (Normal.x + 1.0f)),
                        255.0f * (0.5f * (Normal.y + 1.0f)),
                        255.0f * (0.5f * (Normal.z + 1.0f)),
                        255.0f * Roughness };

            *Pixel++ = (((u32)(Color.a + 0.5f) << 24) |
                ((u32)(Color.r + 0.5f) << 16) |
                ((u32)(Color.g + 0.5f) << 8) |
                ((u32)(Color.b + 0.5f) << 0));
        }
        Row += Bitmap->pitch;
    }
    return res;
}
#else
img make_sphere_normal_map(game_memory_arena* arena,i32 width, i32 height, f32 roughness = 0.f) {
    img res = make_empty_img(arena, width, height);
    f32 inv_width = 1.f / (f32)(width-1);
    f32 inv_height = 1.f / (f32)(height-1);
    u8* row = (u8*)res.mem;
    for (i32 y = 0; y < width; y++) { //TODO(fran): im drawing top down
        u32* pixel = (u32*)row;
        for (i32 x = 0; x < height; x++) {
            //AARRGGBB
            v2 uv = { (f32)x * inv_width, (f32)y * inv_height }; //[0,1]
            //NOTE: this is correct for the x and y positive of the circle (y is down)
            v2 n = uv * 2.f - v2{ 1.f,1.f };//[-1,1]
            f32 root_term = 1.f - squared(n.x) - squared(n.y);
#if 0
            v3 normal{ 0,0,1 };
#else 
            v3 normal{ 0,-.7071067811865475f,-.7071067811865475f };
#endif
            f32 nz = 0;
            if (root_term >= 0) {
                nz = square_root(root_term);
                normal = { n.x,n.y,nz };
            }
            //NOTE: I like this concept of creating your own conversions, I was trying to use i8 to have positive and negative but casey made it simpler by storing u8 and converting to f32 [-1,1]
            v3 color = ((normal + v3{ 1,1,1 }) *.5f )*255.f;//[0,255]

            //NOTE: NEVER use signed integers when working with bytes, when you do shifts it retains the sign and fucks up the whole 32 bits
            *pixel++ = (u8)(roughness * 255.f) << 24 | (u8)round_f32_to_u32(color.r) << 16 | (u8)round_f32_to_u32(color.g) << 8 | (u8)round_f32_to_u32(color.b) << 0;
        }
        row += res.pitch;
    }
    return res;
}
#endif

img make_sphere_diffuse_map(game_memory_arena* arena, i32 width, i32 height) {
    img res = make_empty_img(arena, width, height);
    f32 inv_width = 1.f / (f32)(width - 1);
    f32 inv_height = 1.f / (f32)(height - 1);
    u8* row = (u8*)res.mem;
    for (i32 y = 0; y < width; y++) { //TODO(fran): im drawing top down
        u32* pixel = (u32*)row;
        for (i32 x = 0; x < height; x++) {
            //AARRGGBB
            v2 uv = { (f32)x * inv_width, (f32)y * inv_height }; //[0,1]
            //NOTE: this is correct for the x and y positive of the circle (y is down)
            v2 n = uv * 2.f - v2{ 1.f,1.f };//[-1,1]
            f32 root_term = 1.f - squared(n.x) - squared(n.y);
            f32 alpha=0;
            if (root_term >= 0) {
                alpha = 1;
            }
            v3 color{ .2,.2,.2 }; color *= alpha;
            *pixel++ = (u8)(alpha * 255.f) << 24 | (u8)round_f32_to_u32(color.r*255.f) << 16 | (u8)round_f32_to_u32(color.g * 255.f) << 8 | (u8)round_f32_to_u32(color.b * 255.f) << 0;
        }
        row += res.pitch;
    }
    return res;
}

img make_cylinder_normal_map(game_memory_arena* arena, i32 width, i32 height, f32 roughness = 0.f) {
    img res = make_empty_img(arena, width, height);
    f32 inv_width = 1.f / (f32)(width - 1);
    f32 inv_height = 1.f / (f32)(height - 1);
    u8* row = (u8*)res.mem;
    for (i32 y = 0; y < width; y++) { //TODO(fran): im drawing top down
        u32* pixel = (u32*)row;
        for (i32 x = 0; x < height; x++) {
            //AARRGGBB
            v2 uv = { (f32)x * inv_width, (f32)y * inv_height }; //[0,1]
            //NOTE: this is correct for the x and y positive of the circle (y is down)
            v2 n = hadamard((uv * 2.f - v2{ 1.f,1.f }), {1,0});//[-1,1]
            f32 root_term = 1.f - squared(n.x);
            f32 nz = square_root(root_term);
            v3 normal = { n.x,n.y,nz };
            //NOTE: I like this concept of creating your own conversions, I was trying to use i8 to have positive and negative but casey made it simpler by storing u8 and converting to f32 [-1,1]
            v3 color = ((normal + v3{ 1,1,1 }) * .5f) * 255.f;//[0,255]

            //NOTE: NEVER use signed integers when working with bytes, when you do shifts it retains the sign and fucks up the whole 32 bits
            *pixel++ = (u8)(roughness * 255.f) << 24 | (u8)round_f32_to_u32(color.r) << 16 | (u8)round_f32_to_u32(color.g) << 8 | (u8)round_f32_to_u32(color.b) << 0;
        }
        row += res.pitch;
    }
    return res;
}

u32 color_v4_to_u32(v4 color) {
    color *= 255.f;
    u32 res = round_f32_to_i32(color.a) << 24 | round_f32_to_i32(color.r) << 16 | round_f32_to_i32(color.g) << 8 | round_f32_to_i32(color.b) << 0;
    return res;
}

environment_map make_test_env_map(game_memory_arena* arena, i32 width, i32 height, v4 color) {
    color.rgb *= color.a;
    environment_map res;
    for (i32 idx = 0; idx < arr_count(res.LOD); idx++) {
        res.LOD[idx] = make_empty_img(arena, width, height);
        img* map = &res.LOD[idx];

        bool row_checker_on = true;
        i32 checker_width = 16;
        i32 checker_height = 16;
        for (i32 y = 0; y < map->height; y+=checker_height) { //TODO(fran): im drawing top down
            bool checker_on = row_checker_on;
            for (i32 x = 0; x < map->width; x+=checker_width) {
                v4 c = checker_on ? color : v4{0, 0, 0, 1};
                v2 min = v2_from_i32(x, y);
                v2 max = min + v2{(f32)checker_width, (f32)checker_height};
                game_render_rectangle(map,rc_min_max(min,max),c);
                checker_on = !checker_on;
            }
            row_checker_on = !row_checker_on;
        }

        width /= 2;
        height /= 2;
    }
    return res;
}

void make_test_env_map(environment_map* res, v4 color) {
    color.rgb *= color.a;
    for (i32 idx = 0; idx < arr_count(res->LOD); idx++) {
        
        img* map = &res->LOD[idx];

        bool row_checker_on = true;
        i32 checker_width = 16;
        i32 checker_height = 16;
        for (i32 y = 0; y < map->height; y += checker_height) { //TODO(fran): im drawing top down
            bool checker_on = row_checker_on;
            for (i32 x = 0; x < map->width; x += checker_width) {
                v4 c = checker_on ? color : v4{ 0, 0, 0, 1 };
                v2 min = v2_from_i32(x, y);
                v2 max = min + v2{ (f32)checker_width, (f32)checker_height };
                game_render_rectangle(map, rc_min_max(min, max), c);
                checker_on = !checker_on;
            }
            row_checker_on = !row_checker_on;
        }
    }
}

void game_update_and_render(game_memory* memory, game_framebuffer* frame_buf, game_input* input) {

    //BIG TODO(fran): game_add_entity can no longer copy the game_entity, that would mean it uses the collision areas stored in the saved entity (it's fine for now, until we need to apply some transformation to collision areas)
    
    //TODO(fran): add rgba to v3 and v4, and add xy yz zw, etc union accessors

    //NOTE: handmade day 87 min 45, boolean to tell the game when the exe was reloaded (re-compiled) so it can reset-recalculate some things

    game_assert(sizeof(game_state) <= memory->permanent_storage_sz);
    game_state* gs = (game_state*)memory->permanent_storage;
    
    //INFO IDEA: I see the game, we load stages from the filesystem, each stage can contain stages or levels inside, we show them in a nice rounded squares type menu, you can click to enter any stage/level, then you are sent to the real level

    //NOTE:since our worlds are going to be very small we wont need to do virtualized space like Handmade Hero (day 33), but the idea is there if we decide to reuse the engine for something bigger

    //IDEA: pixel art assets so we can create them fast, Im not sure how well they'll look next to the text for the words, maybe some font can fix that

    //IDEA NOTE: text autocomplete makes you worse at typing, we want to train people to be able to write everything themselves

    //NOTE IDEA: I see an aesthetic with oranges and white, warm colors, reds

    //IDEA: different visible walls for the words to collide against, so it creates sort of a priority system where some words must be typed first cause they are going to collide with a wall that is much closer than the one on the other side of the screen

    //IDEA: I see the walls candy-like red with scrolling diagonal white lines

    //IDEA: screen presents a word in some random position, after n seconds fades to black, then again screen "unfades" and shows a new word in some other position, and so on, the player has to write the word/sentence before the screen turns completely black

    if (!memory->is_initialized) {
        memory->is_initialized = true; //TODO(fran): should the platform layer do this?

        //game_st->hz = 256;
        game_entity null_entity{ 0 };
        null_entity.type = entity_null; //NOTE: just in case
        game_add_entity(gs, &null_entity); //NOTE: entity index 0 is reserved as NULL

        gs->word_height_meters = 1.5f;//TODO(fran): maybe this would be better placed inside world //TODO(fran): handmade day 86 min 45 mentions that it may be good to use values that generate a power of two integer for word_meters_to_pixels //TODO(fran): change to 1.f and simply add it as a note, so we can remove that multiply from all our computations
        gs->word_height_pixels = 60;

        gs->lower_left_pixels = { 0.f,(f32)frame_buf->height };

        gs->word_meters_to_pixels = (f32)gs->word_height_pixels / gs->word_height_meters; //Transform from meters to pixels //NOTE: basis change (same goes for flipping y)

        gs->word_pixels_to_meters = 1 / gs->word_meters_to_pixels;

        initialize_arena(&gs->permanent_arena, (u8*)memory->permanent_storage + sizeof(game_state), memory->permanent_storage_sz - sizeof(game_state));

        gs->world.stages = push_type(&gs->permanent_arena,game_stage);
        gs->world.stage_count = 1;
        gs->world.current_stage = 0;

        gs->world.stages[0].lvls = push_arr(&gs->permanent_arena, game_level_map, 2);
        gs->world.stages[0].level_count = 2; //TODO(fran): can we use sizeof_arr ?? I doubt it
        gs->world.stages[0].current_lvl = 0;

        game_level_map& level1 = gs->world.stages[0].lvls[0];
        game_level_map& level2 = gs->world.stages[0].lvls[1];

#define LVL1_ENTITY_COUNT 3
        level1.entities = push_arr(&gs->permanent_arena, game_entity, LVL1_ENTITY_COUNT);
        level1.entity_count = LVL1_ENTITY_COUNT;

        level2.entities = push_arr(&gs->permanent_arena, game_entity, LVL1_ENTITY_COUNT);
        level2.entity_count = LVL1_ENTITY_COUNT;
        {
            game_entity level1_wall = create_wall(&gs->permanent_arena, v2{ 8.f , 9.5f }*gs->word_height_meters, v2{ .5f , 4.5f }*gs->word_height_meters, { 1.f,0.f,0.f,0.f });

            game_entity level1_player = create_player(&gs->permanent_arena,v2{ 11.f ,5.f }*gs->word_height_meters, v2{ .5f , .5f }*gs->word_height_meters, { .0f,.0f,1.0f,.0f });

            game_entity level1_word_spawner= create_word_spawner(&gs->permanent_arena,v2{ 19.5f, 10.5f } *gs->word_height_meters, v2{ .5f , 4.f } *gs->word_height_meters);

            level1.entities[0] = level1_wall;
            level1.entities[1] = level1_player;
            level1.entities[2] = level1_word_spawner;
        }
        {
            game_entity level2_wall = create_wall(&gs->permanent_arena,v2{ 8.5f , 7.5f }*gs->word_height_meters, v2{ .5f , 4.5f }*gs->word_height_meters, { 1.f,0.f,0.f,.0f });

            game_entity level2_player = create_player(&gs->permanent_arena,v2{ 11.f ,5.f }*gs->word_height_meters, v2{ .75f , .75f }*gs->word_height_meters, { .0f,.0f,1.0f,.0f });

            game_entity level2_word_spawner = create_word_spawner(&gs->permanent_arena,v2{ 16.5f , 10.5f }*gs->word_height_meters, v2{ .5f , .5f }*gs->word_height_meters);

            level2.entities[0] = level2_wall;
            level2.entities[1] = level2_player;
            level2.entities[2] = level2_word_spawner;
        }
        
        gs->DEBUG_background = DEBUG_load_png("assets/img/background.png"); //TODO(fran): release mem DEBUG_unload_png();
        //gs->DEBUG_background.align = { 20,20 };
        gs->DEBUG_menu = DEBUG_load_png("assets/img/braid.png"); //TODO(fran): release mem DEBUG_unload_png();
        //gs->DEBUG_menu.align = { 20,20 };
        gs->DEBUG_mouse = DEBUG_load_png("assets/img/mouse.png"); //TODO(fran): release mem DEBUG_unload_png();
        gs->DEBUG_mouse.alignment_px = v2_from_i32( -gs->DEBUG_mouse.width / 2,gs->DEBUG_mouse.height / 2 );

        gs->word_border = DEBUG_load_png("assets/img/word_border.png");
        gs->word_corner = DEBUG_load_png("assets/img/word_corner.png");
        gs->word_inside = DEBUG_load_png("assets/img/word_inside.png");


        gs->camera = { 0 , 0 }; //TODO(fran): place camera in the world, to simplify first we fix it to the middle of the screen

        game_initialize_entities(gs, gs->world.stages[0].lvls[gs->world.stages[0].current_lvl]);

    }

    game_assert(sizeof(transient_state) <= memory->transient_storage_sz);
    transient_state* ts = (transient_state*)memory->transient_storage;
    if (!ts->is_initialized) {
        ts->is_initialized = true;
        initialize_arena(&ts->transient_arena, (u8*)memory->transient_storage + sizeof(transient_state), ((memory->transient_storage_sz - sizeof(transient_state))/2));

        gs->test_diffuse = make_sphere_diffuse_map(&ts->transient_arena, 256, 256);
        //gs->test_diffuse = make_empty_img(&ts->transient_arena,256, 256);
        //game_render_rectangle(&gs->test_diffuse, rc_min_max({ 0,0 }, v2_from_i32( gs->test_diffuse.width,gs->test_diffuse.height)), {.5f,.5f,.5f,1});
        gs->sphere_normal = make_sphere_normal_map(&ts->transient_arena, gs->test_diffuse.width, gs->test_diffuse.height);
        //gs->sphere_normal = make_cylinder_normal_map(&ts->transient_arena, gs->test_diffuse.width, gs->test_diffuse.height); //TODO(fran): make correct cylinder and pyramid normal maps
        ts->env_map_width = 512;
        ts->env_map_height = 256;

        i32 width= ts->env_map_width, height= ts->env_map_height;
        for (i32 idx = 0; idx < arr_count(ts->TEST_env_map.LOD); idx++) {
            ts->TEST_env_map.LOD[idx] = make_empty_img(&ts->transient_arena, width, height);
            img* map = &ts->TEST_env_map.LOD[idx];
            width /= 2;
            height /= 2;
        }

        //ts->TEST_env_map = make_test_env_map(&ts->transient_arena, ts->env_map_width, ts->env_map_height, { 1.f,0,0,1.f });
        //ts->TEST_env_map.LOD[0] = gs->DEBUG_background;//REMOVE: now thats pretty nice
        //TODO: we can add more data structures to the transient state
        //for example a hash table for the background imgs for the words, lets see if in the same level similar background sizes are used, then we could reduce memory space by reusing the background, at the cost of some performance at the time of searching for the img
    }

    initialize_arena(&gs->one_frame_arena, (u8*)memory->transient_storage + sizeof(transient_state) + ((memory->transient_storage_sz - sizeof(transient_state)) / 2), (memory->transient_storage_sz - sizeof(transient_state)) / 2); //NOTE: this area is reset on each frame //TODO(fran): check if the pointer should be offset by one to avoid stepping into the transient_arena's last byte
    //TODO(fran): we could also create a state structure for this one and do something similar to handmade day 85 (min 49:00) where he adds the security checks for the number of temporary memory areas that are created and destroyed

    //NOTE: I didnt like handmade's idea for the transient arena, since from what I understand, when you start a "sub-arena" it locks the possibility of allocating into the transient (multi frame) section until you exit the sub-arena, and I need to be able to add mem both to the multi and single frame sections (eg the word spawning requires the multiframe arena, so I cannot use a subarena for moving or rendering, depending on when I create the background img for the word)
    //TODO(fran): maybe I should just render from the corners and borders each time, TIME how much slower that would be, if at all, since we also need to take into account that it takes time from the frame to generate the composited img when the img isnt there

    //TODO(fran):objects may need z position to resolve conflicts with render overlapping

    //REMEMBER: you get the input for the previous frame
    
    gs->time += input->dt_sec;

    if (input->controller.back.ended_down) {
        gs->world.stages[0].current_lvl++;
        gs->world.stages[0].current_lvl %= gs->world.stages[0].level_count;

        game_initialize_entities(gs, gs->world.stages[0].lvls[gs->world.stages[0].current_lvl]);
    }

    game_assert(gs->entities[0].type == entity_null);

    //IDEA: what if the platform layer gave you scaling information, so you set up your render like you want and then the only thing that needs to change is scaling

    //Update & Render Prep Loop
    render_group* rg = allocate_render_group(&gs->one_frame_arena,gs->word_meters_to_pixels,&gs->camera,gs->lower_left_pixels,Megabytes(1));

    clear(rg, v4{ .25f,.25f,.25f,1.f });
    //game_render_rectangle(&framebuffer, rc_min_max({ (f32)0,(f32)0 }, { (f32)framebuffer.width,(f32)framebuffer.height }), v4{ 1.f,0.f,1.f,0 });;

    v2 screen_offset = { (f32)frame_buf->width / 2,(f32)frame_buf->height / 2 }; //SUPERTODO: put camera in mtrs space !!!!!!!!!!!!!!!!!!!!!!!!
    //push_img(rg, gs->camera* gs->word_pixels_to_meters + screen_offset * gs->word_pixels_to_meters, &gs->DEBUG_background,true); //TODO(fran): add render order so I can put this guys at the end and pick the current camera pos, not the previous frame one

    for (u32 entity_index = 1; entity_index < gs->entity_count; entity_index++) { //NOTE: remember to start from 1
        game_entity& e= gs->entities[entity_index];
        switch (gs->entities[entity_index].type) {
        case entity_player: 
        {
            v2 dd_word = { 0,0 }; //accel
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
            if (f32 l = length_sq(dd_word); l > 1.f) { //Normalizing the vector, diagonal movement is fixed and also takes care of higher values than 1
                dd_word *= (1.f / sqrtf(l));
            }
            f32 constant_accel = 20.f * gs->word_height_meters; //m/s^2
            dd_word *= constant_accel;
            //TODO(fran): ordinary differential eqs
            dd_word += -3.5f * e.velocity; //basic "drag" //TODO(fran): understand why this doesnt completely eat the acceleration value

            move_entity(&e, dd_word, gs, input); //TODO(fran): for each entity that moves

            v2 screen_offset = { (f32)frame_buf->width / 2,(f32)frame_buf->height / 2 };
            gs->camera = e.pos * gs->word_meters_to_pixels - screen_offset; //TODO(fran): nicer camera update, lerp //TODO(fran): where to update the camera? //TODO(fran): camera position should be on the center, and at the time of drawing account for width and height from there
        } break;
        case entity_wall: break;
        case entity_word: 
        {
            move_entity(&e, {0,0}, gs, input);

            push_img(rg, e.pos, &e.image);

            //stbtt_fontinfo font; //TODO(fran): insert the font again!
            //int width, height;
            //platform_read_entire_file_res f = platform_read_entire_file("c:/windows/fonts/arial.ttf"); //TODO(fran): fonts folder inside /assets
            //stbtt_InitFont(&font, (u8*)f.mem, stbtt_GetFontOffsetForIndex((u8*)f.mem, 0));
            //u8* bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, 50.f), e->txt[0], &width, &height, 0, 0);
            //render_char(&framebuffer,
            //    transform_to_screen_coords(rc_center_radius(e->pos, v2{ (f32)width / 2.f,(f32)height / 2.f }*gs->word_pixels_to_meters),
            //        gs->word_meters_to_pixels, gs->camera, gs->lower_left_pixels), bitmap, width, height);
            //platform_free_file_memory(f.mem);
            //stbtt_FreeBitmap(bitmap, 0);


        } break;
        case entity_word_spawner:
        {
            e.accumulated_time_sec += input->dt_sec;
            if (e.accumulated_time_sec > e.time_till_next_word_sec) {
                e.accumulated_time_sec = 0;
                v2 pos = { lerp(e.pos.x + e.collision.total_area.offset.x - e.collision.total_area.radius.x,e.pos.x + e.collision.total_area.offset.x + e.collision.total_area.radius.x, random_unilateral()),lerp(e.pos.y + e.collision.total_area.offset.y - e.collision.total_area.radius.y,e.pos.y + e.collision.total_area.offset.y + e.collision.total_area.radius.y, random_unilateral()) }; //TODO: probably having a rect struct is better, I can more easily get things like the min point //TODO(fran): pick one of the collision areas and spawn from there, or maybe better to just say we use only one for this guy, since it also needs to generate the direction of the word, and then we'd need to store different directions
#if 0
                game_entity word = create_word(gs,&ts->transient_arena,pos, v2{ .5f , .5f  }*gs->word_height_meters, v2{ -1,0 }*(5 * gs->word_height_meters)* random_unilateral(), { random_unilateral(),random_unilateral() ,random_unilateral() ,random_unilateral() });
                
                game_add_entity(gs, &word);
#endif
            }
        } break;
        case entity_null: game_assert(0);
        default: game_assert(0);
        }

        push_rect_boundary(rg, rc_center_radius(e.pos + e.collision.total_area.offset, e.collision.total_area.radius),e.color);//TODO(fran): iterate over all collision areas and render each one //TODO(fran): bounding box should render above imgs
    }

    img framebuffer;
    framebuffer.height = frame_buf->height;
    framebuffer.width = frame_buf->width;
    framebuffer.mem = frame_buf->mem;
    framebuffer.pitch = frame_buf->pitch;


    push_img(rg, gs->camera * gs->word_pixels_to_meters + screen_offset * gs->word_pixels_to_meters - v2{ (f32)framebuffer.width/2.f-(f32)gs->DEBUG_menu.width / 2.f,(f32)framebuffer.height/2 - gs->DEBUG_menu.height / 2 }*gs->word_pixels_to_meters, &gs->DEBUG_menu);

    
    f32 mousey = (gs->camera.y + framebuffer.height - gs->DEBUG_mouse.height - input->controller.mouse.y) * gs->word_pixels_to_meters;
    f32 mousex = (gs->camera.x + input->controller.mouse.x) * gs->word_pixels_to_meters;

    push_img(rg, { mousex,mousey}, &gs->DEBUG_mouse);

    //game_render_img_ignore_transparency(&framebuffer,  , &gs->DEBUG_background);
    //NOTE or TODO(fran): stb gives the png in correct orientation by default, I'm not sure whether that's gonna cause problems with our orientation reversing
    //game_render_img(&framebuffer, , &gs->DEBUG_menu,.5f); //NOTE: it's pretty interesting that you can actually see information in the pixels with full alpha, the program that generates them does not put rgb to 0 so you can see "hidden" things

    //NOTE: now when we go to render we have to transform from meters, the unit everything in our game is, to pixels, the unit of the screen
    
#if 1
    v2 origin = screen_offset +100.f * v2{ sinf(gs->time) ,2*cos(gs->time) };
#else
    v2 origin = screen_offset +100.f * v2{ 3*sinf(gs->time) ,0 };
#endif
    v4 color{0,1.f,0,0.5f+0.5f*cosf(gs->time*3)}; //NOTE: remember cos goes from [-1,1] we gotta move that to [0,1] for color

    //push_img(rg, {2,5}, &ts->TEST_env_map.LOD[0]);

    v2 x_axis = 256*v2{ cosf(gs->time),sinf(gs->time) };//(100.f/*+50.f*cosf(gs->time)*/)*v2{cosf(gs->time),sinf(gs->time)};
    v2 y_axis = /*(256.f * (1.5f + .5f * sinf(gs->time))) * perp(normalize(x_axis));*/perp(x_axis);//(1+ sinf(gs->time))*perp(x_axis);//NOTE:we will not support skewing/shearing for now //(100.f + 50.f * sinf(gs->time)) *v2{cosf(gs->time+2.f),sinf(gs->time + 2.f) };//NOTE: v2 y_axis = (100.f + 50.f * sinf(gs->time)) *v2{cosf(gs->time-2.f),sinf(gs->time + 2.f) }; == 3d?

    make_test_env_map(&ts->TEST_env_map, { 1.f,0,0,1.f });

    environment_map* top_env_map = &ts->TEST_env_map;
    top_env_map->p_z = -1.f;


    environment_map bottom_env_map = *top_env_map;
    bottom_env_map.p_z = 1.f;

    render_entry_coordinate_system* c = push_coord_system(rg, origin, x_axis, y_axis, color, &gs->test_diffuse, &gs->sphere_normal, top_env_map, &bottom_env_map);
    u32 idx = 0;
    for (f32 y = 0; y < 1.f; y += .25f)
        for (f32 x = 0; x < 1.f; x += .25f)
            c->points[idx++] = {x,y};
    
    render_entry_coordinate_system* lod = push_coord_system(rg, { 200,800 }, 3*v2{ (f32)ts->TEST_env_map.LOD[0].width/6,0 }, 3*v2{ 0,(f32)ts->TEST_env_map.LOD[0].height / 6 }, color, &ts->TEST_env_map.LOD[0], 0, 0,0);

    //Render Loop
    output_render_group(rg,&framebuffer);

    //game_render_img(&framebuffer, { (f32)(input->controller.mouse.x - gs->DEBUG_mouse.alignment_px.x),(f32)(input->controller.mouse.y- gs->DEBUG_mouse.alignment_px.x) }, &gs->DEBUG_mouse);

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












//void game_render_rectangle_boundary(img* buf, rc2 rc, v4 color) {
//
//    f32 thickness = 2.f;
//
//    min_max_v2 m;
//    m.min = rc.get_min();
//    m.max.x = m.min.x + thickness; m.max.y = rc.get_max().y;
//    game_render_rectangle(buf, m, color);//left
//    m.min = rc.get_min(); m.min.x += thickness;
//    m.max.x= rc.get_max().x - thickness; m.max.y = rc.get_min().y + thickness;//TODO(fran): here down is up again
//    game_render_rectangle(buf, m, color);//top
//    m.min.x = rc.get_min().x + thickness; m.min.y = rc.get_max().y - thickness;
//    m.max = rc.get_max(); m.max.x -= thickness;
//    game_render_rectangle(buf, m, color);//bottom
//    m.min.x = rc.get_max().x - thickness; m.min.y = rc.get_min().y;
//    m.max = rc.get_max();
//    game_render_rectangle(buf, m, color);//right
//}


//Render Loop
    //for (u32 i = 1; i < gs->entity_count; i++) {
    //    game_entity* e = &gs->entities[i];
    //    switch (e->type) {
    //    case entity_word:
        //{
            //game_render_img(&framebuffer, transform_to_screen_coords(e->pos,gs->word_meters_to_pixels,gs->camera,gs->lower_left_pixels), &e->image); 
        //    stbtt_fontinfo font;
        //    int width, height; 
        //    platform_read_entire_file_res f = platform_read_entire_file("c:/windows/fonts/arial.ttf"); 
        //    stbtt_InitFont(&font, (u8*)f.mem, stbtt_GetFontOffsetForIndex((u8*)f.mem, 0));
        //    u8* bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, 50.f), e->txt[0], &width, &height, 0, 0);
        //    render_char(&framebuffer,
        //        transform_to_screen_coords(rc_center_radius( e->pos , v2{(f32)width/2.f,(f32)height/2.f}*gs->word_pixels_to_meters), 
        //            gs->word_meters_to_pixels, gs->camera, gs->lower_left_pixels), bitmap, width, height);
        //    platform_free_file_memory(f.mem);
        //    stbtt_FreeBitmap(bitmap, 0);
        //} break;
        //}
        //game_render_rectangle_boundary(
        //    &framebuffer,
        //    transform_to_screen_coords(rc_center_radius(e->pos + e->collision.total_area.offset , e->collision.total_area.radius), gs),
        //    e->color); 
    //}


/*
#if 0 //old collision detection
    {
        v2 new_pos = .5f*dd_word*powf(input->dt_sec,2.f) + current_level->words[0].velocity * input->dt_sec + current_level->words[0].rect.pos;
        v2 new_word_pos00{ new_pos.x - current_level->words[0].rect.radius.x,new_pos.y - current_level->words[0].rect.radius.y};
        v2 new_word_pos01{ new_pos.x + current_level->words[0].rect.radius.x,new_pos.y - current_level->words[0].rect.radius.y};
        v2 new_word_pos10{ new_pos.x - current_level->words[0].rect.radius.x,new_pos.y + current_level->words[0].rect.radius.y};
        v2 new_word_pos11{ new_pos.x + current_level->words[0].rect.radius.x,new_pos.y + current_level->words[0].rect.radius.y}; //TODO(fran): rc to rc collision

        current_level->words[0].velocity += dd_word * input->dt_sec;

        if (!game_check_collision_point_to_rc(new_word_pos00, current_level->walls[0].rect) &&
            !game_check_collision_point_to_rc(new_word_pos01, current_level->walls[0].rect)&&
            !game_check_collision_point_to_rc(new_word_pos10, current_level->walls[0].rect)&&
            !game_check_collision_point_to_rc(new_word_pos11, current_level->walls[0].rect)) { //TODO(fran): add collision checks for the mid points, if two objs are the same width for example then when they line up perfectly one can enter the other from the top or bottom. Or just move on to a new technique
            //TODO(fran): fix faster diagonal movement once we properly use vectors

           current_level->words[0].rect.pos = new_pos;

           v2 screen_offset = { (f32)frame_buf->width/2,(f32)frame_buf->height/2 };

           game_st->camera = current_level->words[0].rect.pos *game_st->word_meters_to_pixels - screen_offset;
        }
        else {
            v2 wall_normal = {1,0};
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
        v2 pos_delta = .5f*dd_word*powf(input->dt_sec,2.f) + player_entity.velocity * input->dt_sec;//NOTE: we'll start just checking a point
        player_entity.velocity += dd_word * input->dt_sec;//TODO: where does this go? //NOTE: Casey put it here, it had it before calculating pos_delta
        //for each wall...
        rc word = { player_entity.pos,player_entity.radius };
        rc wall = { game_st->entities[2].pos , game_st->entities[2].radius };
        //Following the Minkowski Sum now we reduce the word(player) to a point and expand every other object(wall) by the size of the word, so with no need to change any of the code we get for free collision detection for the entire shape of the word
        wall.radius += word.radius;
        v2 min_wall = wall.pos - wall.radius;
        v2 max_wall = wall.pos + wall.radius;

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
            v2 wall_normal{ 0,0 };

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