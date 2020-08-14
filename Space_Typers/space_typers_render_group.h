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
	v2 origin;
	v2 x_axis;
	v2 y_axis;
	u32 layer_idx; //TODO(fran): img alignment, maybe add v2 offset like handmade does
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
	v4 color;
};

struct render_entry_img {
	render_basis basis;
	img* image;
	//TODO(fran): add color, in the push default to {1,1,1,1}
};

struct render_entry_tileable {
	render_basis basis;
	img* mask;
	img* tile;
	v2 tile_offset_percent;
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

//TODO(fran): compute everything position related in the push_..., we'll re-add the two separate loops, one for entity updating and one for entity render pushing, we can remove the v2* camera and dont have to be making all the computations in the middle of the render loop, downside of course is cache, we are getting the same things in and out more than once

struct render_group {
	v2 monitor_radius_mtrs;

	v2 camera;

	//NOTE: camera parameters:
	//TODO(fran): handmade day 110: debug camera, to allow me to see more of the world outside of the player's view
	f32 focal_length; //distance in meters from the viewer(camera) to the monitor
	f32 z_camera_distance_above_ground; //how far in meters the viewer(camera) is to the point

	f32 meters_to_pixels; //NOTE: translates meters _on the monitor_ into pixels _on the monitor_

	layer_info* layer_nfo;

	u8* push_buffer_base; //NOTE: "a buffer that you just push things on"
	u32 push_buffer_used;
	u32 max_push_buffer_sz;
};

render_group* allocate_render_group(game_memory_arena* arena, v2 camera, u32 max_push_buffer_sz, layer_info* layer_nfo, i32 resolution_px_x, i32 resolution_px_y) {
	render_group* res = push_type(arena, render_group);

	res->push_buffer_base = (u8*)push_sz(arena, max_push_buffer_sz);
	res->push_buffer_used = 0;
	res->max_push_buffer_sz = max_push_buffer_sz;
	
	res->camera = camera;
	
	res->layer_nfo = layer_nfo;

	//TODO(fran): better values for this two, once I have straight to pixels push rendering (so I can check the mouse pos without all this)
	res->focal_length = .5f; //Meters from the person to their monitor
	res->z_camera_distance_above_ground = 10.f;

	f32 monitor_width = .6f; //NOTE: horizontal size of monitor in mtrs //approximately based on https://www.dell.com/hr/p/dell-e2720hs-monitor/pd?ref=PD_Family

	res->meters_to_pixels = (f32)resolution_px_x * monitor_width; //NOTE: resolution independent rendering

	f32 pixels_to_meters = 1.f / res->meters_to_pixels;

	res->monitor_radius_mtrs = {resolution_px_x * .5f * pixels_to_meters, resolution_px_y * .5f * pixels_to_meters };

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
void push_rect(render_group* group, v2 origin, v2 x_axis, v2 y_axis, u32 layer_idx, v4 color) { //TODO(fran): check whether it is more useful to use rc2 or two v2
	render_entry_rectangle* p = push_render_element(group,render_entry_rectangle); //TODO(fran): check we received a valid ptr?
	p->basis.origin = origin;
	p->basis.x_axis = x_axis; //TODO(fran): still not sure whether it's better to ask for x_axis or something like x_axis_radius, we'll see when we do more rotation
	p->basis.y_axis = y_axis;
	p->basis.layer_idx = layer_idx;
	p->color = color;
}

void push_img(render_group* group, v2 origin, v2 x_axis, v2 y_axis, u32 layer_idx, img* image) {
	render_entry_img* p = push_render_element(group, render_entry_img); //TODO(fran): check we received a valid ptr?
	p->basis.origin = origin;
	p->basis.x_axis = x_axis;
	p->basis.y_axis = y_axis;
	p->basis.layer_idx = layer_idx;
	p->image = image;
}

void push_rect_boundary(render_group* group, v2 origin, v2 x_axis, v2 y_axis, u32 layer_idx, v4 color) { //TODO(fran): maybe this should be a separate entity instead of 4 push_rects
	f32 thickness= .05f;

	v2 x_thickness = normalize(x_axis) * thickness;
	v2 y_thickness = normalize(y_axis) * thickness;

	//left right
	push_rect(group,origin, x_thickness, y_axis, layer_idx, color); //TODO(fran): why is the correction thickness instead of thickness/2 ?
	push_rect(group, origin + x_axis - x_thickness, x_thickness, y_axis, layer_idx, color);
	//bottom top
	push_rect(group,origin+ x_thickness, x_axis - 2.f* x_thickness, y_thickness, layer_idx, color);
	push_rect(group,origin+y_axis-y_thickness+x_thickness, x_axis - 2.f * x_thickness, y_thickness, layer_idx, color);
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

render_entry_tileable* push_tileable(render_group* group, v2 origin, v2 x_axis, v2 y_axis, u32 layer_idx, img* mask, img* tile, v2 tile_offset_percent, v2 tile_size) {
	render_entry_tileable* p = push_render_element(group, render_entry_tileable); //TODO(fran): check we received a valid ptr?
	p->basis.origin = origin;
	p->basis.x_axis = x_axis;
	p->basis.y_axis = y_axis;
	p->basis.layer_idx = layer_idx;
	p->mask = mask;
	p->tile = tile;
	p->tile_offset_percent = tile_offset_percent;
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
	res.alignment_percent = v2{ 0,0 };
	res.width_over_height = (f32)width / (f32)height;
	res.mem = _push_mem(arena, width * height * IMG_BYTES_PER_PIXEL);
	if (clear_to_zero)
		clear_img(&res);
	return res;
}

void set_bottom_left_alignment(img* image, f32 align_percent_x, f32 align_percent_y) {
	image->alignment_percent = { align_percent_x,align_percent_y };
}

//NOTE: unprojecting the screen
v2 unproject(render_group* rg, v2 projected_xy, f32 at_z_dist_from_cam) { //takes an x y on the screen and converts it to world coordinates
	v2 world_xy = (at_z_dist_from_cam / rg->focal_length) * projected_xy;
	return world_xy;
} //TODO(fran): does this need to take into account the camera position of rg?

rc2 get_camera_rc_at_distance(render_group* rg, f32 z_distance_from_camera) {

	//TODO(fran): does this need to take into account the camera position of rg?

	v2 projected_xy = rg->monitor_radius_mtrs;
	
	v2 raw_xy = unproject(rg, projected_xy, z_distance_from_camera);

	rc2 res = rc_center_radius({ 0,0 }, raw_xy);

	return res;
}

rc2 get_camera_rc_at_target(render_group* rg) { //NOTE: returns mtrs
	rc2 res = get_camera_rc_at_distance(rg,rg->z_camera_distance_above_ground);
	return res;
}