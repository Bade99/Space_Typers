#pragma once

struct render_basis {
	v2 pos;
};

//NOTE: "compact discriminated union"
enum render_group_entry_type {
	RenderGroupEntryType_render_entry_clear,
	RenderGroupEntryType_render_entry_rectangle,
	RenderGroupEntryType_render_entry_img,
	RenderGroupEntryType_render_entry_coordinate_system,
};

struct render_group_entry_header {
	render_group_entry_type type;
};

struct render_entry_clear {
	v4 color;
};
struct render_entry_rectangle{
	render_basis basis;
	rc2 rc;//NOTE: so I dont confuse it
	v4 color;
};

struct render_entry_img {
	render_basis basis;
	img* image;
	v2 center;


	bool IGNOREALPHA; //HACK: get rid of this once we have optimized the renderer, I just use it to be able to render the background img without destroying performance
};

//NOTE: LOD[0] is the highest resolution LOD
struct environment_map {
	img LOD[4];
	f32 p_z; //map's location in z
};

struct render_entry_coordinate_system {
	v2 origin;
	v2 x_axis;
	v2 y_axis;
	v4 color;

	img* texture;
	img* normal_map;

	environment_map* top_env_map;
	environment_map* bottom_env_map;

	v2 points[16];
};

struct render_group {
	f32 meters_to_pixels;
	v2* camera_pixels;
	v2 lower_left_pixels;

	render_basis default_basis;

	u8* push_buffer_base; //NOTE: "a buffer that you just push things on"
	u32 push_buffer_used;
	u32 max_push_buffer_sz;
};

//rc2 transform_to_screen_coords(rc2 game_coords, game_state* gs) {
//	//TODO(fran): now that we use rc this is much simpler to compute than this, no need to go to min-max and then back to center-radius
//	v2 min_pixels = v2{ game_coords.center.x - game_coords.radius.x , game_coords.center.y + game_coords.radius.y }*gs->word_meters_to_pixels;
//
//	v2 min_pixels_camera = min_pixels - gs->camera;
//
//	//INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
//	v2 min_pixels_camera_screen = { gs->lower_left_pixels.x + min_pixels_camera.x , gs->lower_left_pixels.y - min_pixels_camera.y };
//
//	v2 max_pixels = v2{ game_coords.center.x + game_coords.radius.x , game_coords.center.y - game_coords.radius.y }*gs->word_meters_to_pixels;
//
//	v2 max_pixels_camera = max_pixels - gs->camera;
//
//	v2 max_pixels_camera_screen = { gs->lower_left_pixels.x + max_pixels_camera.x , gs->lower_left_pixels.y - max_pixels_camera.y };
//
//	return rc_min_max(min_pixels_camera_screen, max_pixels_camera_screen);
//}

rc2 transform_to_screen_coords(rc2 game_coords, f32 meters_to_pixels, v2 camera_pixels, v2 lower_left_pixels) {
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
v2 transform_to_screen_coords(v2 game_coords, f32 meters_to_pixels, v2 camera_pixels, v2 lower_left_pixels) {
	v2 pixels = v2{ game_coords.x , game_coords.y }*meters_to_pixels;

	v2 pixels_camera = pixels - camera_pixels;

	//INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
	v2 pixels_camera_screen = { lower_left_pixels.x + pixels_camera.x , lower_left_pixels.y - pixels_camera.y };

	return pixels_camera_screen;
}

render_group* allocate_render_group(game_memory_arena* arena, f32 meters_to_pixels, v2* camera_pixels, v2 lower_left_pixels, u32 max_push_buffer_sz) {
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
	sz += sizeof(render_group_entry_header);
	game_assert(group->push_buffer_used+sz < group->max_push_buffer_sz);
	render_group_entry_header* header = (render_group_entry_header*)(group->push_buffer_base + group->push_buffer_used);
	header->type = type;
	void* res = header + 1;//NOTE: pointer arithmetic means this will be header + sizeof(*header)
	group->push_buffer_used += sz;
	return res;
}

//NOTE: I think he used offset as pos, and basis will be useful later for transformations
void push_rect(render_group* group, rc2 rect_mtrs, v4 color) { //TODO(fran): check whether it is more useful to use rc2 or two v2
	render_entry_rectangle* p = push_render_element(group,render_entry_rectangle); //TODO(fran): check we received a valid ptr?
	p->basis = group->default_basis;
	p->color = color;
	p->rc = rect_mtrs;//TODO(fran): should I do the transform here, and live it so the render only offsets by the camera?
}

void push_img(render_group* group, v2 pos_mtrs, img* image, bool IGNOREALPHA=false) {
	render_entry_img* p = push_render_element(group, render_entry_img); //TODO(fran): check we received a valid ptr?
	p->basis = group->default_basis;
	p->center = pos_mtrs;
	p->image = image;

	p->IGNOREALPHA = IGNOREALPHA;
}

void push_rect_boundary(render_group* group, rc2 rect_mtrs, v4 color) {
	f32 thickness=(1/group->meters_to_pixels) *2.f;
	//left right
	push_rect(group, rc_center_radius(rect_mtrs.center - v2{ rect_mtrs.radius.x,0 }, v2{ thickness, rect_mtrs.radius.y }), color); //TODO(fran): this enlarges the rectangle a bit, game_render_rectangle_boundary does it better, benefit here is that it is direction independent
	push_rect(group, rc_center_radius(rect_mtrs.center + v2{ rect_mtrs.radius.x,0 }, v2{ thickness, rect_mtrs.radius.y }) , color );
	//top bottom
	push_rect(group, rc_center_radius(rect_mtrs.center - v2{ 0,rect_mtrs.radius.y }, v2{ rect_mtrs.radius.x, thickness }), color );
	push_rect(group, rc_center_radius(rect_mtrs.center + v2{ 0,rect_mtrs.radius.y }, v2{ rect_mtrs.radius.x, thickness }), color);
}

void clear(render_group* group, v4 color) {
	render_entry_clear* p = push_render_element(group, render_entry_clear); //TODO(fran): check we received a valid ptr?
	p->color = color;
}

render_entry_coordinate_system* push_coord_system(render_group* group, v2 origin, v2 x_axis, v2 y_axis, v4 color, img* texture, img* normal_map,environment_map* top_env_map, environment_map* bottom_env_map) {
	render_entry_coordinate_system* p = push_render_element(group, render_entry_coordinate_system); //TODO(fran): check we received a valid ptr?
	p->origin = origin;
	p->x_axis = x_axis;
	p->y_axis = y_axis;
	p->color = color;
	p->texture = texture;
	p->normal_map = normal_map;
	p->top_env_map = top_env_map;
	p->bottom_env_map = bottom_env_map;
	return p;
}

void game_render_rectangle(img* buf, rc2 rect, v4 color) {
	//The Rectangles are filled NOT including the final row/column
	v2_i32 min = { round_f32_to_i32(rect.get_min().x), round_f32_to_i32(rect.get_min().y) };
	v2_i32 max = { round_f32_to_i32(rect.get_max().x), round_f32_to_i32(rect.get_max().y) };

	if (min.x < 0)min.x = 0;
	if (min.y < 0)min.y = 0;
	if (max.x > buf->width)max.x = buf->width;
	if (max.y > buf->height)max.y = buf->height;

	u32 col = (round_f32_to_u32(color.a * 255.f) << 24) |
		(round_f32_to_u32(color.r * 255.f) << 16) |
		(round_f32_to_u32(color.g * 255.f) << 8) |
		round_f32_to_u32(color.b * 255.f);

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

v4 unpack_4x8(u32 packed) {
	v4 res = { (f32)((packed >> 16) & 0xFF),
			   (f32)((packed >> 8) & 0xFF),
			   (f32)((packed >> 0) & 0xFF),
			   (f32)((packed >> 24) & 0xFF) };
	return res;
}

struct bilinear_sample {
	v4 texel00;
	v4 texel01;
	v4 texel10;
	v4 texel11;
};

bilinear_sample sample_bilinear(img* texture, i32 x, i32 y) {
	u8* texel_ptr = ((u8*)texture->mem + y * texture->pitch + x * IMG_BYTES_PER_PIXEL);

	u32 tex_a = *(u32*)texel_ptr; //NOTE: For a better blend we pick the colors around the texel, movement is much smoother
	u32 tex_b = *(u32*)(texel_ptr + IMG_BYTES_PER_PIXEL);
	u32 tex_c = *(u32*)(texel_ptr + texture->pitch);
	u32 tex_d = *(u32*)(texel_ptr + texture->pitch + IMG_BYTES_PER_PIXEL);

	bilinear_sample res;

	res.texel00 = unpack_4x8(tex_a);
	res.texel01 = unpack_4x8(tex_b);
	res.texel10 = unpack_4x8(tex_c);
	res.texel11 = unpack_4x8(tex_d);
	return res;
}

v4 srgb255_to_linear1(v4 v) {
	//Apply 2.2, aka 2, gamma correction
	v4 res;
	//NOTE: we assume alpha to be in linear space, probably it is
	f32 inv255 = 1.f / 255.f;
	res.r = squared(v.r * inv255);
	res.g = squared(v.g * inv255);
	res.b = squared(v.b * inv255);
	res.a = v.a * inv255;
	return res;
}

bilinear_sample srgb255_to_linear1(bilinear_sample b) {
	bilinear_sample res;
	res.texel00 = srgb255_to_linear1(b.texel00);
	res.texel01 = srgb255_to_linear1(b.texel01);
	res.texel10 = srgb255_to_linear1(b.texel10);
	res.texel11 = srgb255_to_linear1(b.texel11);
	return res;
}

v4 linear1_to_srgb255(v4 v) {
	v4 res;
	//NOTE: we assume alpha to be in linear space, probably it is
	res.r = square_root(v.r) * 255.f;
	res.g = square_root(v.g) * 255.f;
	res.b = square_root(v.b) * 255.f;
	res.a = v.a * 255.f;
	return res;
}

//NOTE: pos will be the center of the image
void game_render_img(img* buf, v2 pos, img* image, f32 dimming = 1.f) { //NOTE: interesting idea: the thing that we read from and the thing that we write to are handled symmetrically (img-img)

	//NOTE: see handmade day 87 for a very nice way of generating seamless img chunks thanks to the pseudo randomness and ordered rendering, example of good arquitecting/planning

	game_assert(dimming >= 0.f && dimming <= 1.f);

	rc2 rec = rc_center_radius(pos, v2_from_i32(image->width / 2, image->height / 2));

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
			v4 texel = { (f32)((*img_pixel >> 16) & 0xFF), 
						 (f32)((*img_pixel >> 8) & 0xFF),
						 (f32)((*img_pixel >> 0) & 0xFF),
						 (f32)((*img_pixel >> 24) & 0xFF) }; //source
			texel = srgb255_to_linear1(texel);
			texel *= dimming;//NOTE: dimming also has to premultiply

			v4 d = { (f32)((*pixel >> 16) & 0xFF),
					 (f32)((*pixel >> 8) & 0xFF),
					 (f32)((*pixel >> 0) & 0xFF),
					 (f32)((*pixel >> 24) & 0xFF) }; //dest

			d = srgb255_to_linear1(d);

			//REMEMBER: handmade day 83, finally understanding what premultiplied alpha is and what it is used for

			//TODO(fran): look at sean barrett's article on premultiplied alpha, remember: "non premultiplied alpha does not distribute over linear interpolation"

			v4 res = (1.f - texel.a) * d + texel;

			res = linear1_to_srgb255(res);

			*pixel = round_f32_to_u32(res.a) << 24 | round_f32_to_u32(res.r) << 16 | round_f32_to_u32(res.g) << 8 | round_f32_to_u32(res.b) << 0; //TODO(fran): should use round_f32_to_u32?

			pixel++;
			img_pixel++;
		}
		row += buf->pitch;
		img_row += image->pitch; //NOTE: now Im starting to see the benefits of using pitch, very easy to change rows when you only have to display parts of the img
	}

}

void game_render_img_ignore_transparency(img* buf, v2 pos, img* image) { 
	rc2 rec = rc_center_radius(pos, v2_from_i32(image->width / 2, image->height / 2));

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

//NOTE: goes from [0,255] to [-1,1]
v4 unscale_and_bias_normal(v4 normal) //TODO(fran): I dont like this name at all
{
	v4 res;
	f32 inv255 = 1.f / 255.f;

	res.x = 2.f * (normal.x * inv255) - 1.f;
	res.y = 2.f * (normal.y * inv255) - 1.f;
	res.z = 2.f * (normal.z * inv255) - 1.f;

	res.w = inv255 * normal.w;

	return res;
}

//NOTE: handmade 99 1:15:00 simple explanation of saturation

#define BACKGROUND_IN_FRONT 0

v3 sample_environment_map(environment_map* env_map, v2 screen_space_uv, v3 sample_direction/*aka the bounce*/, f32 roughness, f32 z_distance_from_map=0.f) {
	//NOTE: screen_space_uv tells where the ray is beign cast from (or to, in my logic) in normalized screen coordinates
	//		sample_direction tells in what direction the cast is going, not required to be normalized (for top-bottom reflection scheme we expect y to be positive)
	//		roughness says which LOD to sample from, at 0 we sample from the highest resolution one, at 1 from the lowest

	u32 LOD_idx = round_f32_to_u32(roughness * ((f32)arr_count(env_map->LOD) - 1));
	img* LOD = &env_map->LOD[LOD_idx];

	//NOTE: handmade 100 8:00 explanation of the direction of the eye vector and other light/normal related things
	//REMEMBER: you shoot to the eye, the eye doesnt shoot, it does not produce light, just receives (the eye vector goes to the eye, NOT from)


	f32 z_distance = 1.f;//meters //distance from the map
	if (z_distance_from_map != 0.f)z_distance = z_distance_from_map;
#if !BACKGROUND_IN_FRONT
	f32 meters_to_uv = .03f;
	f32 c = (z_distance*meters_to_uv) / sample_direction.y; //TODO(fran): isnt that wrong, since z_distance is going straight and z also has the offset from x and y?
	v2 offset = c * v2{ sample_direction.x,sample_direction.z };
#else
	if (dot({ 0,0,1 }, sample_direction) < 0) return{ 0,0,0 };
	//if (dot({ 0,0,1 }, sample_direction) < 0.1) return{ 0,0,0 };
	//if (dot({ 0,0,1 }, sample_direction) >= 0) return{ 0,0,0 };
	//if (dot({ 0,0,1 }, sample_direction) > -0.5) return{ 0,0,0 };
	f32 meters_to_uv = .03f;
	f32 c = (z_distance * meters_to_uv) / (sample_direction.z); //The env map is positioned in the same place as the player, so we have to move in z to reach it //we compute how many Zs enter into the distance to the env map 
	v2 offset = c * v2{ sample_direction.x,sample_direction.y };
	//if ((squared(sample_direction.x) + squared(sample_direction.y))>squared(1.f)  ) return { 0,0,0 };
	//if (fabs(sample_direction.z) < .05f) return{ 0,0,0 };
#endif
	v2 uv = screen_space_uv + offset;

	uv.x = clamp01(uv.x);//avoid sampling outside the texture
	uv.y = clamp01(uv.y);

	f32 t_x = uv.x * (f32)(LOD->width - 2);
	f32 t_y = uv.y * (f32)(LOD->height - 2);

	i32 i_x = (i32)t_x;
	i32 i_y = (i32)t_y;

	f32 f_x = t_x - (f32)i_x;
	f32 f_y = t_y - (f32)i_y;

	game_assert(i_x >= 0 && i_x < LOD->width);
	game_assert(i_y >= 0 && i_y < LOD->height);

	bilinear_sample sample = sample_bilinear(LOD, i_x, i_y);
	sample = srgb255_to_linear1(sample);
	v3 res = lerp(lerp(sample.texel00, sample.texel01, f_x), lerp(sample.texel10, sample.texel11, f_x), f_y).xyz;

#if 0
	*(u32*)((u8*)LOD->mem + i_x * IMG_BYTES_PER_PIXEL + i_y * LOD->pitch) = 0xFFFFFFFF; //REMEMBER: now THAT is visualization, I love it, very easy to see the problem
#endif

	return res;	
}

//TODO(fran): Im pretty sure I have a bug with alpha premult or gamma correction
void game_render_rectangle(img* buf, v2 origin, v2 x_axis, v2 y_axis, v4 color/*tint*/, img* texture, img* normal_map, environment_map* top_env_map, environment_map* bottom_env_map, f32 pixels_to_meters) {
	//NOTE: handmade day 90 to 92
	color.rgb *= color.a; //premultiplication for the color up front

	f32 inv_x_axis_length_sq = 1.f / length_sq(x_axis);
	f32 inv_y_axis_length_sq = 1.f / length_sq(y_axis);

	f32 x_axis_lenght= length(x_axis);
	f32 y_axis_lenght= length(y_axis);

	v2 n_x_axis = y_axis_lenght * (x_axis / x_axis_lenght); //transforms for correct direction of normals on non uniform scaling and rotation: you retain the direction of the axis but scale with the other one (remember that normals exist in a perpendicular space, therefore get modified in a "perpendicular" way)
	v2 n_y_axis = x_axis_lenght * (y_axis / y_axis_lenght);
	f32 n_z_scale = .5f*(x_axis_lenght + y_axis_lenght); //attempt to maintain z on a similar scale to what it would be after scaling the x and y normal axes
	//TODO(fran): better n_z_scale calculation, could also be a parameter sent by the user

	i32 width_max = buf->width-1;
	i32 height_max = buf->height-1;

	f32 inv_width_max = 1.f / (f32)width_max;
	f32 inv_height_max=1.f/(f32)height_max;

	f32 origin_z = 0.f;
	f32 origin_y = (origin + .5f * x_axis + .5f * y_axis).y;
	f32 fixed_cast_y = inv_height_max*origin_y;

	i32 x_min= buf->width;
	i32 x_max= 0;
	i32 y_min= buf->height;
	i32 y_max= 0;

	v2 points[4] = { origin,origin + x_axis,origin + y_axis,origin + x_axis + y_axis };
	for (v2 p : points) {
		i32 floorx=(i32)floorf(p.x);
		i32 ceilx= (i32)ceilf(p.x);
		i32 floory= (i32)floorf(p.y);
		i32 ceily= (i32)ceilf(p.y);

		if (x_min > floorx)x_min = floorx;
		if (y_min > floory)y_min = floory;
		if (x_max < ceilx)x_max = ceilx;
		if (y_max < ceily)y_max = ceily;
	}

	if (x_min < 0)x_min = 0;
	if (y_min < 0)y_min = 0;
	if (x_max > buf->width)x_max = buf->width;
	if (y_max > buf->height)y_max = buf->height;

	u8* row = (u8*)buf->mem + x_min * IMG_BYTES_PER_PIXEL + y_min * buf->pitch;

	//NOTE: Subsurface Scattering reduced explanation handmade day 96 27:00

	//NOTE: Ambient Occlusion handmade day 96 1:25:00
	// -not a real physical thing, just a hack to kind of reproduce real occlusion of light cause of objects in between you and the source
	// -ssao (screen space)

	//NOTE on pixel art rendering (handmade 93 1:07:00): possible solutions:
	// -pixel shader, check pixels that fall between more than one texel and operate differently on them
	// -multisampling (msaa)
	// -manual pre-upsampling

	//NOTE: handmade 94 very intersting talk about gamma and how by not taking that into account you are actually not working on "brightness" space (eg sRGB) but in some other space entirely that is linear, which is wrong for how we perceive brightness changes
	//artist(srgb) -> math (convert to linear for modification and back to non linear for output) -> monitor(srgb)
	//artist -> srgb to linear -> math -> linear to srgb -> monitor
	//normal transform is to square by 2.2, since we want performance we gonna cheat and use 2 as the power, pretty close
	//NOTE 2: day 94 again, really good, the framebuffer needs to have a defined color space too, and using linear is dificult cause we'd need 16bits per channel since we are no longer placing most of the real info where it belongs, on the darker colors, now it is evenly distributed, which means we have too much resolution for bright areas and too little for dark ones

	//TODO(fran): read https://fgiesen.wordpress.com/category/graphics-pipeline/
	//TODO(fran): read https://www.slideshare.net/naughty_dog/lighting-shading-by-john-hable

	for (int y = y_min; y < y_max; y++) {
		u32* pixel = (u32*)row;
		for (int x = x_min; x < x_max; x++) {
			//AARRGGBB
			//NOTE: normals come out of the shape

			//NOTE: you compute the normal for each edge and then check whether the point gets projected in the negative regime, which means that it is "behind the normal" or better behind the edge, aka inside the surface
			//NOTE: this requires orthogonal axes
			v2 p = v2_from_i32( x,y );
			v2 d = p - origin;
			if (dot(d, -perp(x_axis)) < 0 && //bottom edge
				dot(d - x_axis, //NOTE:you line it up to the right edge of the rectangle, you create a point the passes through that line
					-perp(y_axis)) < 0 //NOTE: remember that the perp may point to the other side (right hand rule)
				&&//right edge
				dot(d - x_axis - y_axis, perp(x_axis)) < 0 &&//top edge //NOTE: you can also just use y_axis, cause all you need to do is "line up" the point with the line
				dot(d - y_axis, perp(y_axis)) < 0 ) //left edge //NOTE: same here, you can just use origin
			{
				f32 u = clamp01(dot(d, x_axis) * inv_x_axis_length_sq); //Texture mapping
				f32 v = clamp01(dot(d, y_axis) * inv_y_axis_length_sq);

				f32 t_x = u * (f32)(texture->width - 2); //TODO(fran): avoid reading below and to the right of the buffer in a better way
				f32 t_y = v * (f32)(texture->height - 2);

				i32 i_x = (i32)t_x;
				i32 i_y = (i32)t_y;

				f32 f_x = t_x - (f32)i_x;
				f32 f_y = t_y - (f32)i_y;

				game_assert(i_x >= 0 && i_x < texture->width);
				game_assert(i_y >= 0 && i_y < texture->height);

				//NOTE: Bilinear filtering  //NOTE: For a better blend we pick the colors around the texel, movement is much smoother
				bilinear_sample sample = sample_bilinear(texture, i_x , i_y);

				//NOTE: gamma/space correction
				//go from sRGB to "linear" brightness space
				//NOTE: colors are already premultiplied in linear space (when we load the imgs)
				sample = srgb255_to_linear1(sample);

				//REMEMBER: good to know what to test with, he set up a sprite moving slowly from left to right to see the effect better, and also then a simple rotation
#if 1
				v4 texel = lerp(lerp(sample.texel00,sample.texel01,f_x),lerp(sample.texel10,sample.texel11,f_x),f_y); //TODO(fran): join sample_bilinear, srgb255_to_linear1 and the lerp together into one function, I often forget to apply some of those three
#else 
				v4 texel = texel_a;
#endif

#if BACKGROUND_IN_FRONT
				v2 screen_space_uv = { (f32)x / (f32)(buf->width-1), (f32)y / (f32)(buf->height-1) }; //REMEMBER IMPORTANT OH GOD: CONVERT INTEGERS TO FLOAT, I took me 2 hours to find this bug, INTEGER DIVIDE I HATE U
#else
				v2 screen_space_uv = { inv_width_max*(f32)x,fixed_cast_y };
#endif

				//NOTE: normals are transformed by A inverse transpose (A^-1^t), not A

				if (normal_map) { //TODO(fran): my reflections dont look the same as casey, dont really know where the problem is, I've either got a bug with the sampling code or something external, the bug doesnt seem to be inside this routine (handmade day 103)
					//f32 n_x = u * (f32)(normal_map->width - 2);
					//f32 n_y = v * (f32)(normal_map->height - 2);

					//i32 i_n_x = (i32)n_x;
					//i32 i_n_y = (i32)n_y;

					//f32 f_n_x = n_x - (f32)i_n_x;
					//f32 f_n_y = n_y - (f32)i_n_y;

					bilinear_sample normal_sample = sample_bilinear(normal_map, (i32)t_x, (i32)t_y);
					v4 normal = lerp(lerp(normal_sample.texel00, normal_sample.texel01,f_x),lerp(normal_sample.texel10, normal_sample.texel11,f_x),f_y); //TODO(fran): shouldnt we renormalize after lerp?
					normal = unscale_and_bias_normal(normal);//converts to [-1,1]

					//NOTE: handmade 102 15:00 explanation of how to transform normals on scaling
					//NOTE: z is not modified since we dont have 3d rotations, only x and y, but you do need to scale z so its contribution stays the same as it did before, otherwise x and y might become too big, in which case z will be basically 0, or too small, where z will take over 
					
#if 1
					//NOTE: Rotation and non uniform scale correction
					normal.xy = normal.x * n_x_axis + normal.y * n_y_axis ;
					normal.z *= n_z_scale;
#else
					//NOTE: just the rotation correction
					normal.xy = normal.x * x_axis + normal.y * y_axis;
					normal.z *= .5f*(length(x_axis)+ length(y_axis));
#endif
					//NOTE: normal gets de-normalized after lerping, we should renormalize after unscale_and_bias though we might not need to for the sample_env_amp
					normal.xyz = normalize(normal.xyz);
					
					//v3 vector_to_eye = { 0,0,1 }; //points straight off the screen to the player //NOTE: the fact that this doesnt change allows for optimizations, we can simply do:
					v3 bounce = 2.f * normal.z * normal.xyz;//simplification from -e+2*dot(e,N)*N
					bounce.z -= 1;

					//NOTE: handmade 98 1:07:00 bump map explanation
#if 1
#if !BACKGROUND_IN_FRONT
					bounce.z = -bounce.z;
					f32 z_diff = ((f32)y-origin_y)*pixels_to_meters;
					environment_map* farmap = 0;
					f32 p_z = origin_z + z_diff ;
					f32 tenvmap = bounce.y;
					f32 tfarmap = 0;
					if (tenvmap < -.5f) {
						farmap = &bottom_env_map[0];//bottom
						tfarmap = -1 - 2.f * tenvmap;
					}
					else if (tenvmap > .5f) {
						farmap = &top_env_map[0];//top
						tfarmap = 2.f * (tenvmap - .5f);
					}
					tfarmap *= tfarmap;

					v3 reflection_color = { 0,0,0 };
					if (farmap) {
						f32 z_distance_from_map = farmap->p_z - p_z;
						v3 farmapcolor = sample_environment_map(farmap, screen_space_uv, bounce, normal.w, z_distance_from_map);
						reflection_color = lerp(reflection_color, farmapcolor, tfarmap);
					}
#else				

					v3 reflection_color;
					if (env_map) reflection_color = sample_environment_map(top_env_map, screen_space_uv, bounce, normal.w);
					else reflection_color = { 0,0,0 };
#endif

					texel.rgb += texel.a* reflection_color;
#else //show bounce direction //also pretty cool to see the visual representation of the human eye seeing more detail in the dark areas, the whole img is a gradient but the bright parts look very similar to me while the dark ones have the gradient very clearly visible
					texel.rgb = v3{ .5f,.5f,.5f } + .5f * bounce; //REMEMBER: this is what visualization techniques is about
					texel.rgb *= texel.a;
					/*texel.r *= 0;
					texel.g *= 1;
					texel.b *= 0;
					texel.a = 1;*/
#endif

					texel.r = clamp01(texel.r);
					texel.g = clamp01(texel.g);
					texel.b = clamp01(texel.b);
				}

				//very slow linear blend

				//NOTE: this is the "shader" code
#if 0
				texel = hadamard(texel, color);
#endif
				//

				//NOTE: frambuffer is in sRGB space
				v4 dest = unpack_4x8(*pixel);

				dest = srgb255_to_linear1(dest);

				//REMEMBER: handmade day 83, finally understanding what premultiplied alpha is and what it is used for

				//TODO(fran): look at sean barrett's article on premultiplied alpha, remember: "non premultiplied alpha does not distribute over linear interpolation"

				//(1.f - texel.a) == remaining alpha
				v4 blended = (1.f - texel.a) * dest + texel;
				//NOTE: before premultiplied alpha we did f32 r = rem_r_s_a * d_r + r_s_a * s_r; now we dont need to multiply the source, it already comes premultiplied
				//NOTE: color has to be multiplied by alpha to be premultiplied like the texel is
				//final premultiplication step (handmade day 83 min 1:23:00)

				v4 blended255 = linear1_to_srgb255(blended);

				*pixel = round_f32_to_i32(blended255.a) << 24 | round_f32_to_i32(blended255.r) << 16 | round_f32_to_i32(blended255.g) << 8 | round_f32_to_i32(blended255.b) << 0; //TODO(fran): should use round_f32_to_u32?

			}
			pixel++;
		}
		row += buf->pitch;
	}
}

void output_render_group(render_group* rg, img* output_target) {
	for (u32 base = 0; base < rg->push_buffer_used;) { //TODO(fran): use the basis, and define what is stored in meters and what in px
		render_group_entry_header* header = (render_group_entry_header*)(rg->push_buffer_base + base);
		base += sizeof(*header);
		void* data = header + 1;
		//NOTE: handmade day 89 introduced a set of common parts of every render_entry struct, we might do that too

		switch (header->type) {
		case RenderGroupEntryType_render_entry_clear:
		{
			render_entry_clear* entry = (render_entry_clear*)data;
			game_render_rectangle(output_target, rc_min_max({ 0,0 }, v2_from_i32(output_target->width,output_target->height)), entry->color);
			base += sizeof(*entry); //TODO(fran): annoying to have to remember to add this
		} break;
		case RenderGroupEntryType_render_entry_rectangle:
		{
			render_entry_rectangle* entry = (render_entry_rectangle*)data;
			base += sizeof(*entry);

			rc2 rect = transform_to_screen_coords(entry->rc, rg->meters_to_pixels,*rg->camera_pixels,rg->lower_left_pixels);
			game_render_rectangle(output_target, rect, entry->color);
		} break;
		case RenderGroupEntryType_render_entry_img:
		{
			render_entry_img* entry = (render_entry_img*)data;
			base += sizeof(*entry);
			v2 pos = transform_to_screen_coords(entry->center, rg->meters_to_pixels, *rg->camera_pixels, rg->lower_left_pixels);
			game_assert(entry->image);
			pos -= entry->image->alignment_px;
			if (entry->IGNOREALPHA) game_render_img_ignore_transparency(output_target, pos, entry->image);
			else game_render_img(output_target, pos, entry->image);
		} break;
		case RenderGroupEntryType_render_entry_coordinate_system:
		{
			render_entry_coordinate_system* entry = (render_entry_coordinate_system*)data;
			base += sizeof(*entry);

			game_render_rectangle(output_target, entry->origin, entry->x_axis,entry->y_axis, entry->color, entry->texture, entry->normal_map, entry->top_env_map,entry->bottom_env_map, 1.f/rg->meters_to_pixels);
			
			v2 center = entry->origin;
			v2 radius = { 2,2 };
			game_render_rectangle(output_target, rc_center_radius(center,radius), entry->color);
			center = entry->origin + entry->x_axis;
			game_render_rectangle(output_target, rc_center_radius(center,radius), entry->color);
			center = entry->origin + entry->y_axis;
			game_render_rectangle(output_target, rc_center_radius(center, radius), entry->color);
			center = entry->origin + entry->x_axis + entry->y_axis;
			game_render_rectangle(output_target, rc_center_radius(center, radius), entry->color);


			//for (u32 idx = 0; idx < arr_count(entry->points); idx++) {
			//	v2 p = entry->points[idx];
			//	center = entry->origin + p.x*entry->x_axis + p.y*entry->y_axis;
			//	game_render_rectangle(output_target, rc_center_radius(center, radius), entry->color);
			//}


		} break;
		default: game_assert(0); break;
		}

	}
}