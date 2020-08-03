#pragma once

struct render_basis {
	v2_f32 pos;
};

//NOTE: "compact discriminated union"
enum render_group_entry_type {
	RenderGroupEntryType_render_entry_clear,
	RenderGroupEntryType_render_entry_rectangle,
	RenderGroupEntryType_render_entry_img,
};

struct render_group_entry_header {
	render_group_entry_type type;
};

struct render_entry_clear {
	render_group_entry_header header;
	v4_f32 color;
};
struct render_entry_rectangle{
	render_group_entry_header header;
	render_basis basis;
	rc2 rc;//NOTE: so I dont confuse it
	v4_f32 color;
};

struct render_entry_img {
	render_group_entry_header header;
	render_basis basis;
	img* image;
	v2_f32 center;


	bool IGNOREALPHA; //HACK: get rid of this once we have optimized the renderer, I just use it to be able to render the background img without destroying performance
};

struct render_group {
	f32 meters_to_pixels;
	v2_f32* camera_pixels;
	v2_f32 lower_left_pixels;

	render_basis default_basis;

	u8* push_buffer_base; //NOTE: "a buffer that you just push things on"
	u32 push_buffer_used;
	u32 max_push_buffer_sz;
};

rc2 transform_to_screen_coords(rc2 game_coords, game_state* gs) {
	//TODO(fran): now that we use rc this is much simpler to compute than this, no need to go to min-max and then back to center-radius
	v2_f32 min_pixels = v2_f32{ game_coords.center.x - game_coords.radius.x , game_coords.center.y + game_coords.radius.y }*gs->word_meters_to_pixels;

	v2_f32 min_pixels_camera = min_pixels - gs->camera;

	//INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
	v2_f32 min_pixels_camera_screen = { gs->lower_left_pixels.x + min_pixels_camera.x , gs->lower_left_pixels.y - min_pixels_camera.y };

	v2_f32 max_pixels = v2_f32{ game_coords.center.x + game_coords.radius.x , game_coords.center.y - game_coords.radius.y }*gs->word_meters_to_pixels;

	v2_f32 max_pixels_camera = max_pixels - gs->camera;

	v2_f32 max_pixels_camera_screen = { gs->lower_left_pixels.x + max_pixels_camera.x , gs->lower_left_pixels.y - max_pixels_camera.y };

	return rc_min_max(min_pixels_camera_screen, max_pixels_camera_screen);
}

rc2 transform_to_screen_coords(rc2 game_coords, f32 meters_to_pixels, v2_f32 camera_pixels, v2_f32 lower_left_pixels) {
	//TODO(fran): now that we use rc this is much simpler to compute than this, no need to go to min-max and then back to center-radius
	rc2 res = game_coords;
	res.center *= meters_to_pixels;
	res.radius *= meters_to_pixels;
	res.center -= camera_pixels;
	res.center.x = lower_left_pixels.x + res.center.x;
	res.center.y = lower_left_pixels.y - res.center.y;
	return res;
}

//NOTE: game_coords means meters
v2_f32 transform_to_screen_coords(v2_f32 game_coords, f32 meters_to_pixels, v2_f32 camera_pixels, v2_f32 lower_left_pixels) {
	v2_f32 pixels = v2_f32{ game_coords.x , game_coords.y }*meters_to_pixels;

	v2_f32 pixels_camera = pixels - camera_pixels;

	//INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
	v2_f32 pixels_camera_screen = { lower_left_pixels.x + pixels_camera.x , lower_left_pixels.y - pixels_camera.y };

	return pixels_camera_screen;
}

render_group* allocate_render_group(game_memory_arena* arena, f32 meters_to_pixels, v2_f32* camera_pixels, v2_f32 lower_left_pixels, u32 max_push_buffer_sz) {
	render_group* res = push_type(arena, render_group);

	res->push_buffer_base = (u8*)push_sz(arena, max_push_buffer_sz);
	res->push_buffer_used = 0;
	res->max_push_buffer_sz = max_push_buffer_sz;
	
	res->meters_to_pixels = meters_to_pixels;
	res->camera_pixels = camera_pixels;
	res->lower_left_pixels = lower_left_pixels;
	
	render_basis default_basis;
	default_basis.pos = { 0,0 };
	res->default_basis = default_basis;

	return res;
}

//TODO(fran): neat trick but more annoying than anything else, now the enums are huge
#define push_render_element(group,type) (type*)_push_render_element(group,sizeof(type),RenderGroupEntryType_##type)
void* _push_render_element(render_group* group, u32 sz, render_group_entry_type type) {
	game_assert(group->push_buffer_used+sz < group->max_push_buffer_sz);
	render_group_entry_header* res = 0;
	res = (render_group_entry_header*)(group->push_buffer_base + group->push_buffer_used);
	res->type = type;
	group->push_buffer_used += sz;
	return res;
}

//NOTE: I think he used offset as pos, and basis will be useful later for transformations
void push_rect(render_group* group, rc2 rect_mtrs, v4_f32 color) { //TODO(fran): check whether it is more useful to use rc2 or two v2_f32
	render_entry_rectangle* p = push_render_element(group,render_entry_rectangle); //TODO(fran): check we received a valid ptr?
	p->basis = group->default_basis;
	p->color = color;
	p->rc = rect_mtrs;//TODO(fran): should I do the transform here, and live it so the render only offsets by the camera?
}

void push_img(render_group* group, v2_f32 pos_mtrs, img* image, bool IGNOREALPHA=false) {
	render_entry_img* p = push_render_element(group, render_entry_img); //TODO(fran): check we received a valid ptr?
	p->basis = group->default_basis;
	p->center = pos_mtrs;
	p->image = image;

	p->IGNOREALPHA = IGNOREALPHA;
}

void push_rect_boundary(render_group* group, rc2 rect_mtrs, v4_f32 color) {
	f32 thickness=(1/group->meters_to_pixels) *2.f;
	//left right
	push_rect(group, rc_center_radius(rect_mtrs.center - v2_f32{ rect_mtrs.radius.x,0 }, v2_f32{ thickness, rect_mtrs.radius.y }), color); //TODO(fran): this enlarges the rectangle a bit, game_render_rectangle_boundary does it better, benefit here is that it is direction independent
	push_rect(group, rc_center_radius(rect_mtrs.center + v2_f32{ rect_mtrs.radius.x,0 }, v2_f32{ thickness, rect_mtrs.radius.y }) , color );
	//top bottom
	push_rect(group, rc_center_radius(rect_mtrs.center - v2_f32{ 0,rect_mtrs.radius.y }, v2_f32{ rect_mtrs.radius.x, thickness }), color );
	push_rect(group, rc_center_radius(rect_mtrs.center + v2_f32{ 0,rect_mtrs.radius.y }, v2_f32{ rect_mtrs.radius.x, thickness }), color);
}

void clear(render_group* group, v4_f32 color) {
	render_entry_clear* p = push_render_element(group, render_entry_clear); //TODO(fran): check we received a valid ptr?
	p->color = color;
}

void game_render_rectangle(img* buf, rc2 rect, v4_f32 color) {
	//The Rectangles are filled NOT including the final row/column
	v2_i32 min = { round_f32_to_i32(rect.get_min().x), round_f32_to_i32(rect.get_min().y) };
	v2_i32 max = { round_f32_to_i32(rect.get_max().x), round_f32_to_i32(rect.get_max().y) };

	if (min.x < 0)min.x = 0;
	if (min.y < 0)min.y = 0;
	if (max.x > buf->width)max.x = buf->width;
	if (max.y > buf->height)max.y = buf->height;

	u32 col = (round_f32_to_i32(color.a * 255.f) << 24) |
		(round_f32_to_i32(color.r * 255.f) << 16) |
		(round_f32_to_i32(color.g * 255.f) << 8) |
		round_f32_to_i32(color.b * 255.f);

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

//NOTE: pos will be the center of the image
void game_render_img(img* buf, v2_f32 pos, img* image, f32 dimming = 1.f) { //NOTE: interesting idea: the thing that we read from and the thing that we write to are handled symmetrically (img-img)

	//NOTE: see handmade day 87 for a very nice way of generating seamless img chunks thanks to the pseudo randomness and ordered rendering, example of good arquitecting/planning

	game_assert(dimming >= 0.f && dimming <= 1.f);

	rc2 rec = rc_center_radius(pos, v2_f32_from_i32(image->width / 2, image->height / 2));

	v2_i32 min = { round_f32_to_i32(rec.get_min().x), round_f32_to_i32(rec.get_min().y) };//TODO: cleanup, make simpler
	v2_i32 max = { round_f32_to_i32(rec.get_max().x), round_f32_to_i32(rec.get_max().y) };

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

			//very slow linear blend
			f32 s_a = (f32)((*img_pixel >> 24) & 0xFF); //source
			f32 s_r = dimming * (f32)((*img_pixel >> 16) & 0xFF); //NOTE: dimming also has to premultiply
			f32 s_g = dimming * (f32)((*img_pixel >> 8) & 0xFF);
			f32 s_b = dimming * (f32)((*img_pixel >> 0) & 0xFF);

			f32 r_s_a = (s_a / 255.f) * dimming; //TODO(fran): add extra alpha reduction for every pixel CAlpha (handmade)

			f32 d_a = (f32)((*pixel >> 24) & 0xFF); //dest
			f32 d_r = (f32)((*pixel >> 16) & 0xFF);
			f32 d_g = (f32)((*pixel >> 8) & 0xFF);
			f32 d_b = (f32)((*pixel >> 0) & 0xFF);

			f32 r_d_a = d_a / 255.f;

			//REMEMBER: handmade day 83, finally understanding what premultiplied alpha is and what it is used for

			//TODO(fran): look at sean barrett's article on premultiplied alpha, remember: "non premultiplied alpha does not distribute over linear interpolation"

			f32 rem_r_s_a = (1.f - r_s_a); //remaining alpha
			f32 a = 255.f * (r_s_a + r_d_a - r_s_a * r_d_a); //final premultiplication step (handmade day 83 min 1:23:00)
			f32 r = rem_r_s_a * d_r + s_r;//NOTE: before premultiplied alpha we did f32 r = rem_r_s_a * d_r + r_s_a * s_r; now we dont need to multiply the source, it already comes premultiplied
			f32 g = rem_r_s_a * d_g + s_g;
			f32 b = rem_r_s_a * d_b + s_b;

			*pixel = round_f32_to_i32(a) << 24 | round_f32_to_i32(r) << 16 | round_f32_to_i32(g) << 8 | round_f32_to_i32(b) << 0; //TODO(fran): should use round_f32_to_u32?

			pixel++;
			img_pixel++;
		}
		row += buf->pitch;
		img_row += image->pitch; //NOTE: now Im starting to see the benefits of using pitch, very easy to change rows when you only have to display parts of the img
	}

}

void game_render_img_ignore_transparency(img* buf, v2_f32 pos, img* image) { 
	rc2 rec = rc_center_radius(pos, v2_f32_from_i32(image->width / 2, image->height / 2));

	v2_i32 min = { round_f32_to_i32(rec.get_min().x), round_f32_to_i32(rec.get_min().y) };//TODO: cleanup, make simpler
	v2_i32 max = { round_f32_to_i32(rec.get_max().x), round_f32_to_i32(rec.get_max().y) };

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

void output_render_group(render_group* rg, img* output_target) {
	for (u32 base = 0; base < rg->push_buffer_used;) { //TODO(fran): use the basis, and define what is stored in meters and what in px
		render_group_entry_header* header = (render_group_entry_header*)(rg->push_buffer_base + base);
		//NOTE: handmade day 89 introduced a set of common parts of every render_entry struct, we might do that too

		switch (header->type) {
		case RenderGroupEntryType_render_entry_clear:
		{
			render_entry_clear* entry = (render_entry_clear*)header;
			game_render_rectangle(output_target, rc_min_max({ 0,0 }, v2_f32_from_i32(output_target->width,output_target->height)), entry->color);
			base += sizeof(*entry);
		} break;
		case RenderGroupEntryType_render_entry_rectangle:
		{
			render_entry_rectangle* entry = (render_entry_rectangle*)header;
			base += sizeof(*entry);

			rc2 rect = transform_to_screen_coords(entry->rc, rg->meters_to_pixels,*rg->camera_pixels,rg->lower_left_pixels);
			game_render_rectangle(output_target, rect, entry->color);
		} break;
		case RenderGroupEntryType_render_entry_img:
		{
			render_entry_img* entry = (render_entry_img*)header;
			base += sizeof(*entry);

			v2_f32 pos = transform_to_screen_coords(entry->center, rg->meters_to_pixels, *rg->camera_pixels, rg->lower_left_pixels);
			game_assert(entry->image);
			pos -= entry->image->alignment_px;
			if (entry->IGNOREALPHA) game_render_img_ignore_transparency(output_target, pos, entry->image);
			else game_render_img(output_target, pos, entry->image);
		} break;
		default: game_assert(0); break;
		}

	}
}