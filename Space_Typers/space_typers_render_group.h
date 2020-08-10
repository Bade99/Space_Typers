#pragma once

/*NOTE:
	1. Everywhere y should go up and x to the right, that includes all imgs and render targets
	(aka the mem pointer points to the bottom-most row when viewed on screen)
	2. All inputs to the renderer are in world coordiantes unless specified otherwise, anything
	in pixel values will be explicitly marked (for example with the suffix _px)
	3. All color values sent to the renderer as v4 are in NON premultiplied alpha
*/

//RENDERER API:

//TODO: decide what to do with Z for render order, finite number of layers?, f32?

struct render_basis {
	v2 pos;
};

//NOTE: "compact discriminated union"
enum render_group_entry_type {
	RenderGroupEntryType_render_entry_clear,
	RenderGroupEntryType_render_entry_rectangle,
	RenderGroupEntryType_render_entry_img,
	RenderGroupEntryType_render_entry_coordinate_system,
	RenderGroupEntryType_render_entry_tileable,
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
	//TODO(fran): add color, in the push default to {1,1,1,1}


	bool IGNOREALPHA; //HACK: get rid of this once we have optimized the renderer, I just use it to be able to render the background img without destroying performance
};

struct render_entry_tileable {
	v2 origin;
	v2 x_axis;
	v2 y_axis;
	img* mask;
	img* tile;
	v2 tile_offset_px;
	v2 tile_size;
	//TODO(fran): tile rotation
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

void push_rect_boundary(render_group* group, rc2 rect_mtrs, v4 color) { //TODO(fran): maybe this should be a separate entity instead of 4 push_rects
	f32 thickness=(1/group->meters_to_pixels) *2.f;
	//left right
	push_rect(group, rc_center_radius(rect_mtrs.center - v2{ rect_mtrs.radius.x,0 } + v2{ thickness ,0}, v2{ thickness, rect_mtrs.radius.y }), color); //TODO(fran): why is the correction thickness instead of thickness/2 ?
	push_rect(group, rc_center_radius(rect_mtrs.center + v2{ rect_mtrs.radius.x,0 } - v2{ thickness ,0 }, v2{ thickness, rect_mtrs.radius.y }) , color );
	//bottom top
	push_rect(group, rc_center_radius(rect_mtrs.center - v2{ 0,rect_mtrs.radius.y } + v2{ 0,thickness }, v2{ rect_mtrs.radius.x, thickness }), color );
	push_rect(group, rc_center_radius(rect_mtrs.center + v2{ 0,rect_mtrs.radius.y } - v2{ 0,thickness }, v2{ rect_mtrs.radius.x, thickness }), color);
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

render_entry_tileable* push_tileable(render_group* group, v2 origin, v2 x_axis, v2 y_axis, img* mask, img* tile, v2 tile_offset_px, v2 tile_size) {
	render_entry_tileable* p = push_render_element(group, render_entry_tileable); //TODO(fran): check we received a valid ptr?
	p->origin = origin;
	p->x_axis = x_axis;
	p->y_axis = y_axis;
	p->mask = mask;
	p->tile = tile;
	p->tile_offset_px = tile_offset_px;
	p->tile_size = tile_size;
	return p;
}

void clear_img(img* image) {
	if (image->mem)
		zero_mem(image->mem, image->width * image->height * IMG_BYTES_PER_PIXEL);
}

img make_empty_img(game_memory_arena* arena, u32 width, u32 height, bool clear_to_zero = true) {//TODO(fran): should allow for negative sizes?
	img res;
	res.width = width;
	res.height = height;
	res.pitch = width * IMG_BYTES_PER_PIXEL;
	res.alignment_px = v2{ 0,0 };
	res.mem = _push_mem(arena, width * height * IMG_BYTES_PER_PIXEL);
	if (clear_to_zero)
		clear_img(&res);
	return res;
}

//NOTE: sets the rendering "center" of the img, with no alignment imgs are rendered from the center, use align_px to change that center to any point in the img, choose values relative to the bottom left corner
//Examples: align_px = {(img->width-1)/2,(img->height-1)/2} the center is still the same
//					 = {0,0} "moves" the point {0,0} to the center 
void set_bottom_up_alignment(img* image, v2 align_px) {
	image->alignment_px.x = align_px.x - (f32)(image->width - 1) / 2.f;
	image->alignment_px.y = align_px.y - (f32)(image->height - 1) / 2.f;
}
