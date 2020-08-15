#include "space_typers_render_group.h"

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

	//TODO(fran): add handling for rotated rectangles

	u8* row = (u8*)buf->mem + min.y * buf->pitch + min.x * IMG_BYTES_PER_PIXEL;
	for (int y = min.y; y < max.y; y++) {
		u32* pixel = (u32*)row;
		for (int x = min.x; x < max.x; x++) {
			//AARRGGBB
			*pixel++ = col;
		}
		row += buf->pitch;
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
	//NOTE: handmade 108 1:20:00 interesting, once you scale an img to half size or more you get sampling errors cause you're no longer sampling all the pixels involved, that's when you need MIPMAPPING

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

	rc2 rec = rc_center_radius(pos, v2_from_i32(image->width / 2, image->height / 2)); //TODO(fran): I think this is wrong, width and height should be subtracted 1 (eg. (f32)(width-1))

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

v3 sample_environment_map(environment_map* env_map, v2 screen_space_uv, v3 sample_direction/*aka the bounce*/, f32 roughness, f32 z_distance_from_map = 0.f) {
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
	f32 c = (z_distance * meters_to_uv) / sample_direction.y; //TODO(fran): isnt that wrong, since z_distance is going straight and z also has the offset from x and y?
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

#if 1
	* (u32*)((u8*)LOD->mem + i_x * IMG_BYTES_PER_PIXEL + i_y * LOD->pitch) = 0xFFFFFFFF; //REMEMBER: now THAT is visualization, I love it, very easy to see the problem
#endif

	return res;
}

//NOTE(fran): takes about 280 cycles per pixel (including normal_map which is mostly unused)
//TODO(fran): Im pretty sure I have a bug with alpha premult or gamma correction
//TODO(fran): our challenge will be using AVX instead of SSE2
void game_render_rectangle(img* buf, v2 origin, v2 x_axis, v2 y_axis, v4 color/*tint*/, img* texture, img* normal_map, environment_map* top_env_map, environment_map* bottom_env_map, f32 pixels_to_meters) {
	START_TIMED_BLOCK(game_render_rectangle);//TODO(fran): timer addition
	//NOTE: handmade day 90 to 92
	color.rgb *= color.a; //premultiplication for the color up front

	f32 inv_x_axis_length_sq = 1.f / length_sq(x_axis);
	f32 inv_y_axis_length_sq = 1.f / length_sq(y_axis);

	f32 x_axis_lenght = length(x_axis);
	f32 y_axis_lenght = length(y_axis);

	v2 n_x_axis = y_axis_lenght * (x_axis / x_axis_lenght); //transforms for correct direction of normals on non uniform scaling and rotation: you retain the direction of the axis but scale with the other one (remember that normals exist in a perpendicular space, therefore get modified in a "perpendicular" way)
	v2 n_y_axis = x_axis_lenght * (y_axis / y_axis_lenght);
	f32 n_z_scale = .5f * (x_axis_lenght + y_axis_lenght); //attempt to maintain z on a similar scale to what it would be after scaling the x and y normal axes
	//TODO(fran): better n_z_scale calculation, could also be a parameter sent by the user

	i32 width_max = buf->width - 1;
	i32 height_max = buf->height - 1;

	f32 inv_width_max = 1.f / (f32)width_max;
	f32 inv_height_max = 1.f / (f32)height_max;

	f32 origin_y = (origin + .5f * x_axis + .5f * y_axis).y;
	f32 fixed_cast_y = inv_height_max * origin_y;

	i32 x_min = buf->width;
	i32 x_max = 0;
	i32 y_min = buf->height;
	i32 y_max = 0;

	v2 points[4] = { origin,origin + x_axis,origin + y_axis,origin + x_axis + y_axis };
	for (v2 p : points) {
		i32 floorx = (i32)floorf(p.x);
		i32 ceilx = (i32)ceilf(p.x);
		i32 floory = (i32)floorf(p.y);
		i32 ceily = (i32)ceilf(p.y);

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
			v2 p = v2_from_i32(x, y);
			v2 d = p - origin;
			//if (dot(d, -perp(x_axis)) < 0 && //bottom edge
			//	dot(d - x_axis, //NOTE:you line it up to the right edge of the rectangle, you create a point the passes through that line
			//		-perp(y_axis)) < 0 //NOTE: remember that the perp may point to the other side (right hand rule)
			//	&&//right edge
			//	dot(d - x_axis - y_axis, perp(x_axis)) < 0 &&//top edge //NOTE: you can also just use y_axis, cause all you need to do is "line up" the point with the line
			//	dot(d - y_axis, perp(y_axis)) < 0 ) //left edge //NOTE: same here, you can just use origin
			//{
			f32 u = dot(d, x_axis) * inv_x_axis_length_sq; //Texture mapping, handmade day 93 18:00
			f32 v = dot(d, y_axis) * inv_y_axis_length_sq;
			if (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f) { //NOTE REMEMBER: u v gives you pixel filtering for free, no need to check against the edges of the rectangle explicitly
				
				f32 t_x = u * (f32)(texture->width - 2); //TODO(fran): avoid reading below and to the right of the buffer in a better way
				f32 t_y = v * (f32)(texture->height - 2);

				i32 i_x = (i32)t_x;
				i32 i_y = (i32)t_y;

				f32 f_x = t_x - (f32)i_x;
				f32 f_y = t_y - (f32)i_y;

				game_assert(i_x >= 0 && i_x < texture->width);
				game_assert(i_y >= 0 && i_y < texture->height);

				//NOTE: Bilinear filtering  //NOTE: For a better blend we pick the colors around the texel, movement is much smoother
				bilinear_sample sample = sample_bilinear(texture, i_x, i_y);

				//NOTE: gamma/space correction
				//go from sRGB to "linear" brightness space
				//NOTE: colors are already premultiplied in linear space (when we load the imgs)
				sample = srgb255_to_linear1(sample);

				//REMEMBER: good to know what to test with, he set up a sprite moving slowly from left to right to see the effect better, and also then a simple rotation
#if 1
				v4 texel = lerp(lerp(sample.texel00, sample.texel01, f_x), lerp(sample.texel10, sample.texel11, f_x), f_y); //TODO(fran): join sample_bilinear, srgb255_to_linear1 and the lerp together into one function, I often forget to apply some of those three
#else 
				v4 texel = texel_a;
#endif

#if BACKGROUND_IN_FRONT
				v2 screen_space_uv = { (f32)x / (f32)(buf->width - 1), (f32)y / (f32)(buf->height - 1) }; //REMEMBER IMPORTANT OH GOD: CONVERT INTEGERS TO FLOAT, I took me 2 hours to find this bug, INTEGER DIVIDE I HATE U
#else
				v2 screen_space_uv = { inv_width_max * (f32)x,fixed_cast_y };
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
					v4 normal = lerp(lerp(normal_sample.texel00, normal_sample.texel01, f_x), lerp(normal_sample.texel10, normal_sample.texel11, f_x), f_y); //TODO(fran): shouldnt we renormalize after lerp?
					normal = unscale_and_bias_normal(normal);//converts to [-1,1]

					//NOTE: handmade 102 15:00 explanation of how to transform normals on scaling
					//NOTE: z is not modified since we dont have 3d rotations, only x and y, but you do need to scale z so its contribution stays the same as it did before, otherwise x and y might become too big, in which case z will be basically 0, or too small, where z will take over 

#if 1
					//NOTE: Rotation and non uniform scale correction
					normal.xy = normal.x * n_x_axis + normal.y * n_y_axis;
					normal.z *= n_z_scale;
#else
					//NOTE: just the rotation correction
					normal.xy = normal.x * x_axis + normal.y * y_axis;
					normal.z *= .5f * (length(x_axis) + length(y_axis));
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
					f32 z_diff = ((f32)y - origin_y) * pixels_to_meters;
					environment_map* farmap = 0;
					f32 origin_z = 0.f;
					f32 p_z = origin_z + z_diff;
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

					texel.rgb += texel.a * reflection_color;
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
#if 1
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

	END_TIMED_BLOCK(game_render_rectangle);
}

//NOTE: pre anything (but with O2): ~260 cycles per pixel (game_render_rectangle without all the normal map stuff)
//REMEMBER: amazingly enough the program got faster with each "abstraction" removal, no vectorization and already it is at least 50 cycles faster, so yeah Zero Cost Abstractions DONT Exist (at least as of 2020 with the stupid microsoft compiler, test llvm)
	//NOTE: it seems the compiler isnt able to do the precomputations casey understood could be done
#if 0
void game_render_rectangle_fast(img* buf, v2 origin, v2 x_axis, v2 y_axis, v4 color, img* texture, f32 pixels_to_meters) {
	START_TIMED_BLOCK(game_render_rectangle_fast);//TODO(fran): timer addition
	color.rgb *= color.a; //premultiplication for the color up front

	f32 inv_x_axis_length_sq = 1.f / length_sq(x_axis);
	f32 inv_y_axis_length_sq = 1.f / length_sq(y_axis);

	v2 n_x_axis = x_axis * inv_x_axis_length_sq;
	v2 n_y_axis = y_axis * inv_y_axis_length_sq;

	f32 x_axis_lenght = length(x_axis);
	f32 y_axis_lenght = length(y_axis);

	i32 width_max = buf->width - 1;
	i32 height_max = buf->height - 1;

	f32 inv_width_max = 1.f / (f32)width_max;
	f32 inv_height_max = 1.f / (f32)height_max;

	f32 inv255 = 1.f / 255.f;

	f32 one_255 = 255.f;

	i32 x_min = buf->width;
	i32 x_max = 0;
	i32 y_min = buf->height;
	i32 y_max = 0;

	v2 points[4] = { origin,origin + x_axis,origin + y_axis,origin + x_axis + y_axis };
	for (v2 p : points) {
		i32 floorx = (i32)floorf(p.x);
		i32 ceilx = (i32)ceilf(p.x);
		i32 floory = (i32)floorf(p.y);
		i32 ceily = (i32)ceilf(p.y);

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

	for (int y = y_min; y < y_max; y++) {
		u32* pixel = (u32*)row;
		for (int x = x_min; x < x_max; x++) {
			START_TIMED_BLOCK(test_pixel);

			v2 p = { (f32)x, (f32)y };
			v2 d = p - origin;
			f32 u = dot(d, n_x_axis); //Texture mapping, handmade day 93 18:00
			f32 v = dot(d, n_y_axis);
			if (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f) {
				START_TIMED_BLOCK(fill_pixel);

				f32 t_x = u * (f32)(texture->width - 2); //TODO(fran): avoid reading below and to the right of the buffer in a better way
				f32 t_y = v * (f32)(texture->height - 2);

				i32 i_x = (i32)t_x;
				i32 i_y = (i32)t_y;

				f32 f_x = t_x - (f32)i_x;
				f32 f_y = t_y - (f32)i_y;

				game_assert(i_x >= 0 && i_x < texture->width);
				game_assert(i_y >= 0 && i_y < texture->height);

				//sample_bilinear
				u8* texel_ptr = ((u8*)texture->mem + i_y * texture->pitch + i_x * IMG_BYTES_PER_PIXEL);
				u32 packed_A = *(u32*)texel_ptr;
				u32 packed_B = *(u32*)(texel_ptr + IMG_BYTES_PER_PIXEL);
				u32 packed_C = *(u32*)(texel_ptr + texture->pitch);
				u32 packed_D = *(u32*)(texel_ptr + texture->pitch + IMG_BYTES_PER_PIXEL);

				//unpack_4x8 for 4 samples
				f32 unpacked_A_r = (f32)((packed_A >> 16) & 0xFF);
				f32 unpacked_A_g = (f32)((packed_A >> 8) & 0xFF);
				f32 unpacked_A_b = (f32)((packed_A >> 0) & 0xFF);
				f32 unpacked_A_a = (f32)((packed_A >> 24) & 0xFF);

				f32 unpacked_B_r = (f32)((packed_B >> 16) & 0xFF);
				f32 unpacked_B_g = (f32)((packed_B >> 8) & 0xFF);
				f32 unpacked_B_b = (f32)((packed_B >> 0) & 0xFF);
				f32 unpacked_B_a = (f32)((packed_B >> 24) & 0xFF);

				f32 unpacked_C_r = (f32)((packed_C >> 16) & 0xFF);
				f32 unpacked_C_g = (f32)((packed_C >> 8) & 0xFF);
				f32 unpacked_C_b = (f32)((packed_C >> 0) & 0xFF);
				f32 unpacked_C_a = (f32)((packed_C >> 24) & 0xFF);

				f32 unpacked_D_r = (f32)((packed_D >> 16) & 0xFF);
				f32 unpacked_D_g = (f32)((packed_D >> 8) & 0xFF);
				f32 unpacked_D_b = (f32)((packed_D >> 0) & 0xFF);
				f32 unpacked_D_a = (f32)((packed_D >> 24) & 0xFF);

				//srgb255_to_linear1 for 4 samples
				unpacked_A_r = squared(unpacked_A_r * inv255);
				unpacked_A_g = squared(unpacked_A_g * inv255);
				unpacked_A_b = squared(unpacked_A_b * inv255);
				unpacked_A_a = unpacked_A_a * inv255;

				unpacked_B_r = squared(unpacked_B_r * inv255);
				unpacked_B_g = squared(unpacked_B_g * inv255);
				unpacked_B_b = squared(unpacked_B_b * inv255);
				unpacked_B_a = unpacked_B_a * inv255;

				unpacked_C_r = squared(unpacked_C_r * inv255);
				unpacked_C_g = squared(unpacked_C_g * inv255);
				unpacked_C_b = squared(unpacked_C_b * inv255);
				unpacked_C_a = unpacked_C_a * inv255;

				unpacked_D_r = squared(unpacked_D_r * inv255);
				unpacked_D_g = squared(unpacked_D_g * inv255);
				unpacked_D_b = squared(unpacked_D_b * inv255);
				unpacked_D_a = unpacked_D_a * inv255;

				//lerp(lerp(),lerp())
				f32 f_x_rem = 1.f - f_x;
				f32 f_y_rem = 1.f - f_y;
				f32 l0 = f_y_rem * f_x_rem;
				f32 l1 = f_y_rem * f_x;
				f32 l2 = f_y * f_x_rem;
				f32 l3 = f_y * f_x;
				f32 texel_r = l0 * unpacked_A_r + l1 * unpacked_B_r + l2 * unpacked_C_r + l3 * unpacked_D_r;
				f32 texel_g = l0 * unpacked_A_g + l1 * unpacked_B_g + l2 * unpacked_C_g + l3 * unpacked_D_g;
				f32 texel_b = l0 * unpacked_A_b + l1 * unpacked_B_b + l2 * unpacked_C_b + l3 * unpacked_D_b;
				f32 texel_a = l0 * unpacked_A_a + l1 * unpacked_B_a + l2 * unpacked_C_a + l3 * unpacked_D_a;

				//hadamard
				texel_r = texel_r * color.r;
				texel_g = texel_g * color.g;
				texel_b = texel_b * color.b;
				texel_a = texel_a * color.a;

				texel_r = clamp01(texel_r);
				texel_g = clamp01(texel_g);
				texel_b = clamp01(texel_b);
				texel_a = clamp01(texel_a);

				//unpack_4x8
				f32 dest_r = (f32)((*pixel >> 16) & 0xFF);
				f32 dest_g = (f32)((*pixel >> 8) & 0xFF);
				f32 dest_b = (f32)((*pixel >> 0) & 0xFF);
				f32 dest_a = (f32)((*pixel >> 24) & 0xFF);

				//srgb255_to_linear1
				dest_r = squared(dest_r * inv255);
				dest_g = squared(dest_g * inv255);
				dest_b = squared(dest_b * inv255);
				dest_a = dest_a * inv255;

				//v4 blended = (1.f - texel.a) * dest + texel;
				f32 texel_a_rem = (1.f - texel_a);
				f32 blended_r = texel_a_rem * dest_r + texel_r;
				f32 blended_g = texel_a_rem * dest_g + texel_g;
				f32 blended_b = texel_a_rem * dest_b + texel_b;
				f32 blended_a = texel_a_rem * dest_a + texel_a;

				//linear1_to_srgb255
				blended_r = square_root(blended_r) * one_255;
				blended_g = square_root(blended_g) * one_255;
				blended_b = square_root(blended_b) * one_255;
				blended_a = blended_a * one_255;

				//round_f32_to_u32 & write
				*pixel = (u32)(blended_a+.5f) << 24 | (u32)(blended_r+.5f) << 16 | (u32)(blended_g+.5f) << 8 | (u32)(blended_b+.5f) << 0;

				END_TIMED_BLOCK(fill_pixel);
			}
			pixel++;

			END_TIMED_BLOCK(test_pixel);
		}
		row += buf->pitch;
	}

	END_TIMED_BLOCK(game_render_rectangle_fast);
}
#else
#include <immintrin.h> //TODO(fran): check why there are other 5+ intrin headers
//NOTE: we doin' AVX instead of SSE
//NOTE: REMEMBER: visual studio died while I wrote this stuff, clearly not its target user it'd seem, reeeeally slowed down
void game_render_rectangle_fast(img* buf, v2 origin, v2 x_axis, v2 y_axis, v4 color, img* texture, f32 pixels_to_meters) {
	START_TIMED_BLOCK(game_render_rectangle_fast);//TODO(fran): timer addition
	
	color.rgb *= color.a; //premultiplication for the color up front

	f32 inv_x_axis_length_sq = 1.f / length_sq(x_axis);
	f32 inv_y_axis_length_sq = 1.f / length_sq(y_axis);

	v2 n_x_axis = x_axis * inv_x_axis_length_sq;
	v2 n_y_axis = y_axis * inv_y_axis_length_sq;
	
	f32 x_axis_lenght = length(x_axis);
	f32 y_axis_lenght = length(y_axis);

	i32 width_max = buf->width - 1     - 3; //TODO(fran): remove, quick hack for getting avx working and not writing to the other side of the screen
	i32 height_max = buf->height - 1   - 3;

	i32 x_min = width_max;
	i32 x_max = 0;
	i32 y_min = height_max;
	i32 y_max = 0;

	v2 points[4] = { origin,origin + x_axis,origin + y_axis,origin + x_axis + y_axis };
	for (v2 p : points) {
		i32 floorx = (i32)floorf(p.x);
		i32 ceilx = (i32)ceilf(p.x);
		i32 floory = (i32)floorf(p.y);
		i32 ceily = (i32)ceilf(p.y);

		if (x_min > floorx)x_min = floorx;
		if (y_min > floory)y_min = floory;
		if (x_max < ceilx)x_max = ceilx;
		if (y_max < ceily)y_max = ceily;
	}

	if (x_min < 0)x_min = 0;
	if (y_min < 0)y_min = 0;
	if (x_max > width_max)x_max = width_max;
	if (y_max > height_max)y_max = height_max;

	__m256 origin_x = _mm256_set1_ps(origin.x);
	__m256 origin_y = _mm256_set1_ps(origin.y);
	__m256 n_x_axis_x = _mm256_set1_ps(n_x_axis.x);
	__m256 n_x_axis_y = _mm256_set1_ps(n_x_axis.y);
	__m256 n_y_axis_x = _mm256_set1_ps(n_y_axis.x);
	__m256 n_y_axis_y = _mm256_set1_ps(n_y_axis.y);
	__m256 one = _mm256_set1_ps(1.f);
	__m256 zero = _mm256_set1_ps(.0f);
	__m256 one_255 = _mm256_set1_ps(255.f);
	__m256 inv255 = _mm256_set1_ps(1.f / 255.f);
	__m256 color_r = _mm256_set1_ps(color.r);
	__m256 color_g = _mm256_set1_ps(color.g);
	__m256 color_b = _mm256_set1_ps(color.b);
	__m256 color_a = _mm256_set1_ps(color.a);

	u8* row = (u8*)buf->mem + x_min * IMG_BYTES_PER_PIXEL + y_min * buf->pitch;

	START_TIMED_BLOCK(process_pixel);
	for (int y = y_min; y <= y_max; y++) {
		u32* pixel = (u32*)row;
		for (int x_it = x_min; x_it <= x_max; x_it += 8) {

			__m256 texel_A_r = _mm256_set1_ps(.0f);
			__m256 texel_A_g = _mm256_set1_ps(.0f);
			__m256 texel_A_b = _mm256_set1_ps(.0f);
			__m256 texel_A_a = _mm256_set1_ps(.0f);
							 
			__m256 texel_B_r = _mm256_set1_ps(.0f);
			__m256 texel_B_g = _mm256_set1_ps(.0f);
			__m256 texel_B_b = _mm256_set1_ps(.0f);
			__m256 texel_B_a = _mm256_set1_ps(.0f);
							 
			__m256 texel_C_r = _mm256_set1_ps(.0f);
			__m256 texel_C_g = _mm256_set1_ps(.0f);
			__m256 texel_C_b = _mm256_set1_ps(.0f);
			__m256 texel_C_a = _mm256_set1_ps(.0f);
							 
			__m256 texel_D_r = _mm256_set1_ps(.0f);
			__m256 texel_D_g = _mm256_set1_ps(.0f);
			__m256 texel_D_b = _mm256_set1_ps(.0f);
			__m256 texel_D_a = _mm256_set1_ps(.0f);

			bool should_fill[8];

			__m256 dest_r = _mm256_set1_ps(.0f);
			__m256 dest_g = _mm256_set1_ps(.0f);
			__m256 dest_b = _mm256_set1_ps(.0f);
			__m256 dest_a = _mm256_set1_ps(.0f);

			__m256 f_x = _mm256_set1_ps(.0f);
			__m256 f_y = _mm256_set1_ps(.0f);

			__m256 blended_r;
			__m256 blended_g;
			__m256 blended_b;
			__m256 blended_a;

			//NOTE: REMEMBER: (still inside the for loop from 0 to 8) somehow just reducing this 3 (from p_x to u) to their basic form, without doing any SIMD, removed almost 30 cycles, wtf? yet again Zero Cost Abstractions do NOT exist (2020 msvc)
			__m256 p_x = _mm256_set_ps( (f32)(x_it + 7), (f32)(x_it + 6), (f32)(x_it + 5), (f32)(x_it + 4), (f32)(x_it + 3), (f32)(x_it + 2), (f32)(x_it + 1), (f32)(x_it + 0));
			__m256 p_y = _mm256_set1_ps((f32)y);
			//p - origin
			__m256 d_x = _mm256_sub_ps(p_x, origin_x);
			__m256 d_y = _mm256_sub_ps(p_y, origin_y);
			//dot for uv texture mapping
			__m256 u = _mm256_add_ps(_mm256_mul_ps(d_x, n_x_axis_x), _mm256_mul_ps(d_y, n_x_axis_y));
			__m256 v = _mm256_add_ps(_mm256_mul_ps(d_x, n_y_axis_x), _mm256_mul_ps(d_y, n_y_axis_y));

			for (i32 p_idx = 0; p_idx < 8; p_idx++){

				should_fill[p_idx] = u.m256_f32[p_idx] >= 0.f && u.m256_f32[p_idx] <= 1.f && v.m256_f32[p_idx] >= 0.f && v.m256_f32[p_idx] <= 1.f;
				if (should_fill[p_idx]) {
					//START_TIMED_BLOCK(fill_pixel);

					f32 t_x = u.m256_f32[p_idx] * (f32)(texture->width - 2); //TODO(fran): avoid reading below and to the right of the buffer in a better way
					f32 t_y = v.m256_f32[p_idx] * (f32)(texture->height - 2);

					i32 i_x = (i32)t_x;
					i32 i_y = (i32)t_y;

					f_x.m256_f32[p_idx] = t_x - (f32)i_x;
					f_y.m256_f32[p_idx] = t_y - (f32)i_y;

					game_assert(i_x >= 0 && i_x < texture->width);
					game_assert(i_y >= 0 && i_y < texture->height);

					//sample_bilinear
					u8* texel_ptr = ((u8*)texture->mem + i_y * texture->pitch + i_x * IMG_BYTES_PER_PIXEL);
					u32 packed_A = *(u32*)texel_ptr;
					u32 packed_B = *(u32*)(texel_ptr + IMG_BYTES_PER_PIXEL);
					u32 packed_C = *(u32*)(texel_ptr + texture->pitch);
					u32 packed_D = *(u32*)(texel_ptr + texture->pitch + IMG_BYTES_PER_PIXEL);

					//unpack_4x8 for 4 samples
					texel_A_r.m256_f32[p_idx] = (f32)((packed_A >> 16) & 0xFF);
					texel_A_g.m256_f32[p_idx] = (f32)((packed_A >> 8) & 0xFF);
					texel_A_b.m256_f32[p_idx] = (f32)((packed_A >> 0) & 0xFF);
					texel_A_a.m256_f32[p_idx] = (f32)((packed_A >> 24) & 0xFF);
						 	 
					texel_B_r.m256_f32[p_idx] = (f32)((packed_B >> 16) & 0xFF);
					texel_B_g.m256_f32[p_idx] = (f32)((packed_B >> 8) & 0xFF);
					texel_B_b.m256_f32[p_idx] = (f32)((packed_B >> 0) & 0xFF);
					texel_B_a.m256_f32[p_idx] = (f32)((packed_B >> 24) & 0xFF);
							 
					texel_C_r.m256_f32[p_idx] = (f32)((packed_C >> 16) & 0xFF);
					texel_C_g.m256_f32[p_idx] = (f32)((packed_C >> 8) & 0xFF);
					texel_C_b.m256_f32[p_idx] = (f32)((packed_C >> 0) & 0xFF);
					texel_C_a.m256_f32[p_idx] = (f32)((packed_C >> 24) & 0xFF);
							 
					texel_D_r.m256_f32[p_idx] = (f32)((packed_D >> 16) & 0xFF);
					texel_D_g.m256_f32[p_idx] = (f32)((packed_D >> 8) & 0xFF);
					texel_D_b.m256_f32[p_idx] = (f32)((packed_D >> 0) & 0xFF);
					texel_D_a.m256_f32[p_idx] = (f32)((packed_D >> 24) & 0xFF);

					//unpack_4x8
					dest_r.m256_f32[p_idx] = (f32)((*(pixel+p_idx) >> 16) & 0xFF);
					dest_g.m256_f32[p_idx] = (f32)((*(pixel+p_idx) >> 8) & 0xFF);
					dest_b.m256_f32[p_idx] = (f32)((*(pixel+p_idx) >> 0) & 0xFF);
					dest_a.m256_f32[p_idx] = (f32)((*(pixel+p_idx) >> 24) & 0xFF);
				}
			}
			
			//TODO(fran): check that the compiler doesnt compute "a" twice //NOTE: in debug it DOES replicate the operations, the good part is the result is the same since each intermediate result is not stored in any of the operands but in a new variable, so it does do mul(mul,mul) instead of a=mul -> mul(a,a) but they give the same result. 
			//	In release it think it does it right, TODO(fran): might be better to remove the macros and test?
#define _mm256_square_ps(a) _mm256_mul_ps(a,a)

			//srgb255_to_linear1 for 4 samples
			texel_A_r = _mm256_square_ps(_mm256_mul_ps( texel_A_r, inv255));
			texel_A_g = _mm256_square_ps(_mm256_mul_ps( texel_A_g, inv255));
			texel_A_b = _mm256_square_ps(_mm256_mul_ps( texel_A_b, inv255));
			texel_A_a = _mm256_mul_ps(texel_A_a, inv255);

			texel_B_r = _mm256_square_ps(_mm256_mul_ps(texel_B_r, inv255));
			texel_B_g = _mm256_square_ps(_mm256_mul_ps(texel_B_g, inv255));
			texel_B_b = _mm256_square_ps(_mm256_mul_ps(texel_B_b, inv255));
			texel_B_a = _mm256_mul_ps(texel_B_a, inv255);

			texel_C_r = _mm256_square_ps(_mm256_mul_ps(texel_C_r, inv255));
			texel_C_g = _mm256_square_ps(_mm256_mul_ps(texel_C_g, inv255));
			texel_C_b = _mm256_square_ps(_mm256_mul_ps(texel_C_b, inv255));
			texel_C_a = _mm256_mul_ps(texel_C_a, inv255);

			texel_D_r = _mm256_square_ps(_mm256_mul_ps(texel_D_r, inv255));
			texel_D_g = _mm256_square_ps(_mm256_mul_ps(texel_D_g, inv255));
			texel_D_b = _mm256_square_ps(_mm256_mul_ps(texel_D_b, inv255));
			texel_D_a = _mm256_mul_ps(texel_D_a, inv255);

			//lerp(lerp(),lerp())
			__m256 f_x_rem = _mm256_sub_ps(one, f_x);
			__m256 f_y_rem = _mm256_sub_ps(one, f_y);
			__m256 l0 = _mm256_mul_ps(f_y_rem, f_x_rem);
			__m256 l1 = _mm256_mul_ps(f_y_rem, f_x);
			__m256 l2 = _mm256_mul_ps(f_y, f_x_rem);
			__m256 l3 = _mm256_mul_ps(f_y, f_x);
			__m256 texel_r = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(l0, texel_A_r), _mm256_mul_ps(l1, texel_B_r)), _mm256_add_ps(_mm256_mul_ps(l2, texel_C_r), _mm256_mul_ps(l3, texel_D_r)));
			__m256 texel_g = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(l0, texel_A_g), _mm256_mul_ps(l1, texel_B_g)), _mm256_add_ps(_mm256_mul_ps(l2, texel_C_g), _mm256_mul_ps(l3, texel_D_g)));
			__m256 texel_b = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(l0, texel_A_b), _mm256_mul_ps(l1, texel_B_b)), _mm256_add_ps(_mm256_mul_ps(l2, texel_C_b), _mm256_mul_ps(l3, texel_D_b)));
			__m256 texel_a = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(l0, texel_A_a), _mm256_mul_ps(l1, texel_B_a)), _mm256_add_ps(_mm256_mul_ps(l2, texel_C_a), _mm256_mul_ps(l3, texel_D_a)));

			//hadamard
			texel_r = _mm256_mul_ps(texel_r, color_r);
			texel_g = _mm256_mul_ps(texel_g, color_g);
			texel_b = _mm256_mul_ps(texel_b, color_b);
			texel_a = _mm256_mul_ps(texel_a, color_a);

			//clamp01
			texel_r = _mm256_min_ps(_mm256_max_ps(texel_r, zero),one);
			texel_g = _mm256_min_ps(_mm256_max_ps(texel_g, zero),one);
			texel_b = _mm256_min_ps(_mm256_max_ps(texel_b, zero),one);
			texel_a = _mm256_min_ps(_mm256_max_ps(texel_a, zero),one);

			//srgb255_to_linear1
			dest_r = _mm256_square_ps(_mm256_mul_ps(dest_r, inv255));
			dest_g = _mm256_square_ps(_mm256_mul_ps(dest_g, inv255));
			dest_b = _mm256_square_ps(_mm256_mul_ps(dest_b, inv255));
			dest_a = _mm256_mul_ps(dest_a, inv255);

			//v4 blended = (1.f - texel.a) * dest + texel;
			__m256 texel_a_rem = _mm256_sub_ps(one, texel_a);
			blended_r = _mm256_add_ps(_mm256_mul_ps(texel_a_rem, dest_r), texel_r);
			blended_g = _mm256_add_ps(_mm256_mul_ps(texel_a_rem, dest_g), texel_g);
			blended_b = _mm256_add_ps(_mm256_mul_ps(texel_a_rem, dest_b), texel_b);
			blended_a = _mm256_add_ps(_mm256_mul_ps(texel_a_rem, dest_a), texel_a);

			//linear1_to_srgb255
			blended_r = _mm256_mul_ps(_mm256_sqrt_ps(blended_r), one_255);
			blended_g = _mm256_mul_ps(_mm256_sqrt_ps(blended_g), one_255);
			blended_b = _mm256_mul_ps(_mm256_sqrt_ps(blended_b), one_255);
			blended_a = _mm256_mul_ps(blended_a, one_255);

			for (i32 p_idx = 0; p_idx < 8; p_idx++) {
				//round_f32_to_u32 & write
				if(should_fill[p_idx])
					*(pixel+p_idx) = (u32)(blended_a.m256_f32[p_idx] + .5f) << 24 | (u32)(blended_r.m256_f32[p_idx] + .5f) << 16 | (u32)(blended_g.m256_f32[p_idx] + .5f) << 8 | (u32)(blended_b.m256_f32[p_idx] + .5f) << 0;

			}
					//END_TIMED_BLOCK(fill_pixel);
			pixel+=8;
		}
		row += buf->pitch;
	}
	END_TIMED_BLOCK_COUNTED(process_pixel,(x_max-x_min+1)*(y_max - y_min + 1));

	END_TIMED_BLOCK(game_render_rectangle_fast);
}
#endif

//TODO(fran): Im pretty sure I have a bug with alpha premult or gamma correction
void game_render_tileable(img* buf, v2 origin, v2 x_axis, v2 y_axis, img* mask, img* tile, v2 tile_offset_percent, v2 tile_size_px) {

	f32 inv_x_axis_length_sq = 1.f / length_sq(x_axis);
	f32 inv_y_axis_length_sq = 1.f / length_sq(y_axis);

	i32 x_min = buf->width;
	i32 x_max = 0;
	i32 y_min = buf->height;
	i32 y_max = 0;

	v2 points[4] = { origin,origin + x_axis,origin + y_axis,origin + x_axis + y_axis };
	for (v2 p : points) {
		i32 floorx = (i32)floorf(p.x);
		i32 ceilx = (i32)ceilf(p.x);
		i32 floory = (i32)floorf(p.y);
		i32 ceily = (i32)ceilf(p.y);

		if (x_min > floorx)x_min = floorx;
		if (y_min > floory)y_min = floory;
		if (x_max < ceilx)x_max = ceilx;
		if (y_max < ceily)y_max = ceily;
	}

	if (x_min < 0)x_min = 0;
	if (y_min < 0)y_min = 0;
	if (x_max > buf->width)x_max = buf->width;
	if (y_max > buf->height)y_max = buf->height;

	v2 tile_origin = tile_offset_percent; //UV offset
	v2 tile_scale = { length(x_axis) / tile_size_px.x, length(y_axis) / tile_size_px.y };


	u8* row = (u8*)buf->mem + x_min * IMG_BYTES_PER_PIXEL + y_min * buf->pitch;

	//TODO(fran): tiling is not quite right, couple of ideas
	// -loss of float precision
	// -off by 1 error somewhere (seems pretty probable)
	
	for (int y = y_min; y < y_max; y++) {
		u32* pixel = (u32*)row;
		for (int x = x_min; x < x_max; x++) {

			v2 p = v2_from_i32(x, y);
			v2 d = p - origin; //TODO(fran): the first time p and origin are off by .5, maybe that has something to do with the wrong tiling, that gives negative u and v values so the pixel is skipped
			f32 u = dot(d, x_axis) * inv_x_axis_length_sq; 
			f32 v = dot(d, y_axis) * inv_y_axis_length_sq;
			if (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f) {
				f32 t_x = u * (f32)(mask->width - 1); //TODO(fran): avoid reading below and to the right of the buffer
				f32 t_y = v * (f32)(mask->height - 1);

				i32 i_x = (i32)t_x;
				i32 i_y = (i32)t_y;

				f32 f_x = t_x - (f32)i_x;
				f32 f_y = t_y - (f32)i_y;

				//game_assert(i_x >= 0 && i_x < mask->width);
				//game_assert(i_y >= 0 && i_y < mask->height);

				bilinear_sample sample = sample_bilinear(mask, i_x, i_y);

				sample = srgb255_to_linear1(sample);

				v4 texel = lerp(lerp(sample.texel00, sample.texel01, f_x), lerp(sample.texel10, sample.texel11, f_x), f_y);

				if (tile) {
					//Calculate the tile uv with respect to the full uv
					v2 uv{ u,v };
					
					v2 tile_uv = tile_origin + hadamard(uv, tile_scale); //NOTE: pretty sure this is wrong (I feel this will not create the wrapping effect I want)

					//poor man's uv wrapping
					while (tile_uv.u > 1.f)tile_uv.u -= 1.f;
					while (tile_uv.u < 0.f)tile_uv.u += 1.f;
					while (tile_uv.v > 1.f)tile_uv.v -= 1.f;
					while (tile_uv.v < 0.f)tile_uv.v += 1.f;


					f32 tile_x = tile_uv.u * (f32)(tile->width - 1); //TODO(fran): avoid reading below and to the right of the buffer
					f32 tile_y = tile_uv.v * (f32)(tile->height - 1);

					i32 i_tile_x = (i32)tile_x;
					i32 i_tile_y = (i32)tile_y;

					f32 f_tile_x = tile_x - (f32)i_tile_x;
					f32 f_tile_y = tile_y - (f32)i_tile_y;

					bilinear_sample tile_sample = sample_bilinear(tile, (i32)tile_x, (i32)tile_y);
					tile_sample = srgb255_to_linear1(tile_sample);
					v4 tile_px = lerp(lerp(tile_sample.texel00, tile_sample.texel01, f_tile_x), lerp(tile_sample.texel10, tile_sample.texel11, f_tile_x), f_tile_y);

					texel = tile_px * texel.a;
#if 0
					//Visualize tile boundaries
					if (tile_uv.u >= .99f || tile_uv.v >= .99f || tile_uv.u <= .01f || tile_uv.v <= .01f) texel.rgb = { 1.f,0,1.f };
#endif
				}

				v4 dest = unpack_4x8(*pixel);

				dest = srgb255_to_linear1(dest);

				v4 blended = (1.f - texel.a) * dest + texel;

				v4 blended255 = linear1_to_srgb255(blended);

				*pixel = round_f32_to_i32(blended255.a) << 24 | round_f32_to_i32(blended255.r) << 16 | round_f32_to_i32(blended255.g) << 8 | round_f32_to_i32(blended255.b) << 0; //TODO(fran): should use round_f32_to_u32?
			}
			pixel++;
		}
		row += buf->pitch;
	}
}

void game_render_rectangle(img* buf, v2 origin, v2 x_axis, v2 y_axis, v4 color01) {

	color01.rgb *= color01.a; //premult

	f32 inv_x_axis_length_sq = 1.f / length_sq(x_axis);
	f32 inv_y_axis_length_sq = 1.f / length_sq(y_axis);

	i32 x_min = buf->width;
	i32 x_max = 0;
	i32 y_min = buf->height;
	i32 y_max = 0;

	v2 points[4] = { origin,origin + x_axis,origin + y_axis,origin + x_axis + y_axis };
	for (v2 p : points) {
		i32 floorx = (i32)floorf(p.x);
		i32 ceilx = (i32)ceilf(p.x);
		i32 floory = (i32)floorf(p.y);
		i32 ceily = (i32)ceilf(p.y);

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

	for (int y = y_min; y < y_max; y++) {
		u32* pixel = (u32*)row;
		for (int x = x_min; x < x_max; x++) {

			v2 p = v2_from_i32(x, y);
			v2 d = p - origin;
			f32 u = dot(d, x_axis) * inv_x_axis_length_sq;
			f32 v = dot(d, y_axis) * inv_y_axis_length_sq;
			if (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f) {

				v4 dest = unpack_4x8(*pixel);

				dest = srgb255_to_linear1(dest);

				v4 blended = (1.f - color01.a) * dest + color01;

				v4 blended255 = linear1_to_srgb255(blended);

				*pixel = round_f32_to_u32(blended255.a) << 24 | round_f32_to_u32(blended255.r) << 16 | round_f32_to_u32(blended255.g) << 8 | round_f32_to_u32(blended255.b) << 0;
			}
			pixel++;
		}
		row += buf->pitch;
	}
}

//rc2 transform_to_screen_coords(rc2 game_coords, f32 meters_to_pixels, v2 camera_pixels, v2 lower_left_pixels) {
//	//TODO(fran): now that we use rc this is much simpler to compute than this, no need to go to min-max and then back to center-radius
//	rc2 res = game_coords;
//	res.center *= meters_to_pixels;
//	res.radius *= meters_to_pixels;
//	res.center -= camera_pixels;
//	res.center.x = lower_left_pixels.x + res.center.x;
//	res.center.y = lower_left_pixels.y + res.center.y;
//	return res;
//}

//NOTE: game_coords means meters
//v2 transform_to_screen_coords(v2 game_coords, f32 meters_to_pixels, v2 camera_pixels, v2 lower_left_pixels) {
//	v2 pixels = v2{ game_coords.x , game_coords.y }*meters_to_pixels;
//
//	v2 pixels_camera = pixels - camera_pixels;
//
//	//INFO: y is flipped and we subtract the height of the framebuffer so we render in the correct orientation with the origin at the bottom-left
//	v2 pixels_camera_screen = { lower_left_pixels.x + pixels_camera.x , lower_left_pixels.y + pixels_camera.y };
//
//	return pixels_camera_screen;
//}


v2 transform_to_screen_coords(v2 game_coords, f32 meters_to_pixels, v2 camera, v2 screen_center_px, f32 z_scale) {
	v2 pos_relative_to_cam = (game_coords - camera)*z_scale; //offset from the middle of the logical screen
	v2 pos_px = pos_relative_to_cam* meters_to_pixels + screen_center_px;
	return pos_px;
}

struct screen_coords_res {
	v2 p_px;
	bool valid;
	f32 scale_px; //in case you want to scale other things with the same scaling that was applied to the point
};
screen_coords_res transform_to_screen_coords_perspective(v2 game_coords, render_group* rg, v2 screen_center_px, f32 z_scale) {
	screen_coords_res res{0};

	v3 raw_xy = V3( (game_coords - rg->camera) , 1.f);


	f32 z_distance_to_p = rg->z_camera_distance_above_ground - (z_scale-1.f) ; //NOTE: he uses a z that goes positive for things getting closer and negative for things going away, and 0 for no scale cause he uses this values as distances not scale percentages, TODO(fran): decide if I want to do like him, in which case I need to change the current scale of the layers to 0, and here not subtract 1 from z_scale

	f32 near_clip_plane = .1f;

	if (z_distance_to_p > near_clip_plane) {
		v3 projected_xy = rg->focal_length * raw_xy / z_distance_to_p;
		
		res.p_px = screen_center_px + projected_xy.xy * rg->meters_to_pixels;
		res.scale_px = projected_xy.z * rg->meters_to_pixels;
		res.valid = true;
	}

	return res;
} //TODO(fran): perspective doesnt allow for my idea of "the mouse on the top layer with no scaling" to work, if we continue with this we need straight pixel position push_...

rc2 transform_to_screen_coords(rc2 game_coords, f32 meters_to_pixels, v2 camera, v2 screen_center_px, f32 z_scale) {
	//TODO(fran): now that we use rc this is much simpler to compute than this, no need to go to min-max and then back to center-radius
	rc2 res = rc_min_max(transform_to_screen_coords(game_coords.get_min(), meters_to_pixels, camera, screen_center_px, z_scale), transform_to_screen_coords(game_coords.get_max(), meters_to_pixels, camera, screen_center_px, z_scale));
	return res;
}

f32 get_layer_scaling(layer_info* nfo, u32 idx) {
	game_assert(idx < nfo->layer_count);
	f32 res = nfo->layers[idx].current_scale;
	return res;
}

void output_render_group(render_group* rg, img* output_target) {

	START_TIMED_BLOCK(output_render_group);

	v2 screen_dim = v2_from_i32( output_target->width,output_target->height );
	v2 screen_center = screen_dim*.5f;

	f32 pixels_to_meters = 1.f / rg->meters_to_pixels;

	for (u32 base = 0; base < rg->push_buffer_used;) { //TODO(fran): use the basis, and define what is stored in meters and what in px
		render_group_entry_header* header = (render_group_entry_header*)(rg->push_buffer_base + base);
		base += sizeof(*header);
		void* data = header + 1;
		//NOTE: handmade day 89 introduced a set of common parts of every render_entry struct, we might do that too

		switch (header->type) {
		case RenderGroupEntryType_render_entry_clear:
		{
			render_entry_clear* entry = (render_entry_clear*)data;
			base += sizeof(*entry); //TODO(fran): annoying to have to remember to add this
			game_render_rectangle(output_target, rc_min_max({ 0,0 }, v2_from_i32(output_target->width, output_target->height)), entry->color);
		} break;
		case RenderGroupEntryType_render_entry_rectangle:
		{
			render_entry_rectangle* entry = (render_entry_rectangle*)data;
			base += sizeof(*entry);
#if !PERSPECTIVE
			f32 z_scaling = get_layer_scaling(rg->layer_nfo, entry->layer_idx);
			v2 origin = transform_to_screen_coords(entry->rc.get_min(), rg->meters_to_pixels, *rg->camera, screen_center, z_scaling);

			v2 axes = entry->rc.get_max() - entry->rc.get_min();

			v2 x_axis = v2{ axes.x, 0 } *rg->meters_to_pixels * z_scaling;
			v2 y_axis = v2{0, axes .y} *rg->meters_to_pixels* z_scaling;
#else 
			screen_coords_res res = transform_to_screen_coords_perspective(entry->basis.origin, rg, screen_center, get_layer_scaling(rg->layer_nfo, entry->basis.layer_idx));
			if (!res.valid)break;
			v2 origin = res.p_px;
			v2 x_axis = entry->basis.x_axis *res.scale_px;
			v2 y_axis = entry->basis.y_axis *res.scale_px;
#endif
			game_render_rectangle(output_target, origin, x_axis, y_axis, entry->color);

		} break;
		case RenderGroupEntryType_render_entry_img:
		{
			render_entry_img* entry = (render_entry_img*)data;
			base += sizeof(*entry);
#if !PERSPECTIVE
			f32 z_scaling = get_layer_scaling(rg->layer_nfo, entry->layer_idx); //TODO(fran): now we do have a common thing that probably all entries share and could be precalculated before the switch
			v2 pos = transform_to_screen_coords(entry->center, rg->meters_to_pixels, *rg->camera, screen_center, z_scaling);

			v2 x_axis = v2{ 1,0 }*(f32)entry->image->width * z_scaling;
			v2 y_axis = v2{ 0,1 }*(f32)entry->image->height * z_scaling;

			pos -= {length(x_axis)*.5f, length(y_axis)*.5f};

			game_assert(entry->image);
			pos -= entry->image->alignment_px*z_scaling;//NOTE: I think the reason why this is a subtraction is that you want to bring the new center to where you are, it sort of makes sense in my head but im still a bit confused. You are not moving the center to a point, you are bringing a point to the center
#else
			screen_coords_res res = transform_to_screen_coords_perspective(entry->basis.origin, rg, screen_center, get_layer_scaling(rg->layer_nfo, entry->basis.layer_idx));
			if (!res.valid)break;

			v2 x_axis = entry->basis.x_axis * res.scale_px;
			v2 y_axis = entry->basis.y_axis * res.scale_px;

			v2 origin = res.p_px ;

			game_assert(entry->image);
			origin -= (entry->image->alignment_percent.x*x_axis + entry->image->alignment_percent.y * y_axis);
#endif
#if 0
			game_render_rectangle(output_target, origin, x_axis, y_axis, {1,1,1,1}, entry->image, 0, 0, 0, pixels_to_meters);
#else 
			game_render_rectangle_fast(output_target, origin, x_axis, y_axis, { 1,1,1,1 }, entry->image, pixels_to_meters);
#endif
		} break;
		case RenderGroupEntryType_render_entry_coordinate_system:
		{
			render_entry_coordinate_system* entry = (render_entry_coordinate_system*)data;
			base += sizeof(*entry);

			game_render_rectangle(output_target, entry->origin, entry->x_axis, entry->y_axis, entry->color, entry->texture, entry->normal_map, entry->top_env_map, entry->bottom_env_map, pixels_to_meters);

			v2 center = entry->origin;
			v2 radius = { 2,2 };
			game_render_rectangle(output_target, rc_center_radius(center, radius), entry->color);
			center = entry->origin + entry->x_axis;
			game_render_rectangle(output_target, rc_center_radius(center, radius), entry->color);
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
		case RenderGroupEntryType_render_entry_tileable:
		{
			render_entry_tileable* entry = (render_entry_tileable*)data;
			base += sizeof(*entry);
#if !PERSPECTIVE
			f32 z_scaling = get_layer_scaling(rg->layer_nfo, entry->layer_idx);
			v2 origin = transform_to_screen_coords(entry->origin, rg->meters_to_pixels, *rg->camera, screen_center, z_scaling);

			v2 x_axis = entry->x_axis * rg->meters_to_pixels * z_scaling; //TODO(fran): should the axes take into accout rg->lower_left_pixels? in the sense of y flipping, though we dont do that anymore 
			v2 y_axis = entry->y_axis * rg->meters_to_pixels * z_scaling;
			v2 tile_sz_px = entry->tile_size * rg->meters_to_pixels * z_scaling;
#else
			;
			screen_coords_res res = transform_to_screen_coords_perspective(entry->basis.origin, rg, screen_center, get_layer_scaling(rg->layer_nfo, entry->basis.layer_idx));
			if (!res.valid)break;

			v2 origin = res.p_px;
			v2 x_axis = entry->basis.x_axis * res.scale_px;
			v2 y_axis = entry->basis.y_axis * res.scale_px;
			v2 tile_sz_px = entry->tile_size * res.scale_px;
#endif
			game_render_tileable(output_target, origin, x_axis, y_axis, entry->mask, entry->tile, entry->tile_offset_percent, tile_sz_px);
		} break;
		default: game_assert(0); break;
		}

	}

	END_TIMED_BLOCK(output_render_group);
}
//TODO(fran): Im having to convert to origin at the bottom left corner each time I push one of these, not pretty