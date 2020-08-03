#pragma once

struct render_basis {
	v2_f32 pos;
};

struct render_piece {
	render_basis basis;
	img* image;
	v2_f32 center;

	v4_f32 color;
	v2_f32 radius;
};

struct render_group {
	f32 meters_to_pixels;
	render_basis default_basis;

	u8* push_buffer_base; //NOTE: "a buffer that you just push things on"
	u32 push_buffer_used;
	u32 max_push_buffer_sz;
};

render_group* allocate_render_group(game_memory_arena* arena, f32 meters_to_pixels, u32 max_push_buffer_sz) {
	render_group* res = push_type(arena, render_group);

	res->push_buffer_base = (u8*)push_sz(arena, max_push_buffer_sz);
	res->push_buffer_used = 0;
	res->max_push_buffer_sz = max_push_buffer_sz;
	
	res->meters_to_pixels = meters_to_pixels;
	
	render_basis default_basis;
	default_basis.pos = { 0,0 };
	res->default_basis = default_basis;

	return res;
}

void* push_render_element(render_group* group, u32 sz) {
	game_assert(group->push_buffer_used+sz < group->max_push_buffer_sz);
	void* res = 0;
	res = group->push_buffer_base + group->push_buffer_used;
	group->push_buffer_used += sz;
	return res;
}

//NOTE: I think he used offset as pos, and basis will be useful later for transformations
void push_piece(render_group* group, v2_f32 pos_mtrs, img* image, v4_f32 color, v2_f32 radius_mtrs) {
	render_piece* p = (render_piece*)push_render_element(group,sizeof(render_piece));
	p->basis = group->default_basis;
	p->image = image;
	p->color = color;
	p->center = pos_mtrs;//TODO(fran): should I do the transform here, and live it so the render only offsets by the camera?
	p->radius = radius_mtrs; 
}

void push_img(render_group* group, v2_f32 pos_mtrs, img* image) {
	push_piece(group, pos_mtrs, image, v4_f32{ 0,0,0,0 }, v2_f32{ 0,0 });
}

void push_rect(render_group* group, rc2 rect_mtrs, v4_f32 color) {
	push_piece(group, rect_mtrs.center, 0, color, rect_mtrs.radius);
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