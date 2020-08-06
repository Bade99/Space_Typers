#pragma once
#include <random>//TODO(fran):remove
#include <chrono>//TODO(fran):remove

f32 sign(f32 n) {
	f32 res;
	res = n >= 0.f ? 1.f : -1.f;
	return res;
}


//TODO(fran): once we have a centralized random series generator send that to each function and create seed() function (see handmade day 82)
f32 random_unilateral() { //NOTE: random from [0,1]
	//TODO(fran): xorshift + handamde day 434 / ray 02
	std::chrono::system_clock::rep seed = std::chrono::system_clock::now().time_since_epoch().count();
	static std::mt19937 generator((unsigned int)seed);
	std::uniform_real_distribution<> distribution{ 0.f,1.f };
	return (f32)distribution(generator);
}

f32 random_bilateral() { //NOTE: random from [-1,1] //NOTE I would prefer random_-1_1 but that syntax is not valid
	//TODO(fran): xorshift + handamde day 434 / ray 02
	std::chrono::system_clock::rep seed = std::chrono::system_clock::now().time_since_epoch().count();
	static std::mt19937 generator((unsigned int)seed);
	std::uniform_real_distribution<> distribution{ -1.f,1.f };
	return (f32)distribution(generator);
}

f32 random_between(f32 min, f32 max) { //NOTE: random between user specified range [min,max]
	//TODO(fran): xorshift + handamde day 434 / ray 02
	std::chrono::system_clock::rep seed = std::chrono::system_clock::now().time_since_epoch().count();
	static std::mt19937 generator((unsigned int)seed);
	std::uniform_real_distribution<> distribution{ min,max };
	return (f32)distribution(generator);
}

i32 random_between(i32 min, i32 max) { //NOTE: random integer between user specified range [min,max]
	//TODO(fran): xorshift + handamde day 434 / ray 02
	std::chrono::system_clock::rep seed = std::chrono::system_clock::now().time_since_epoch().count();
	static std::mt19937 generator((unsigned int)seed);
	std::uniform_int_distribution<> distribution{ min,max };
	return distribution(generator);
}

u32 random_count(u32 choice_count) { //produces a random integer between [0,choice_count-1]
	//TODO(fran): xorshift + handamde day 434 / ray 02
	std::chrono::system_clock::rep seed = std::chrono::system_clock::now().time_since_epoch().count();
	static std::mt19937 generator((unsigned int)seed);
	std::uniform_int_distribution<> distribution{ 0,(i32)(choice_count-1) }; //TODO(fran): I feel this could be problematic
	return distribution(generator);
}

f32 lerp(f32 n1, f32 n2, f32 t) { //TODO(fran): put t as the middle param?
	//NOTE: interesting that https://en.wikipedia.org/wiki/Linear_interpolation mentions this is the Precise method
	return (1.f - t) * n1 + t * n2;
}

v2 lerp(v2 n1, v2 n2, f32 t) {
	//NOTE: interesting that https://en.wikipedia.org/wiki/Linear_interpolation mentions this is the Precise method
	return (1.f - t) * n1 + t * n2;
}

v3 lerp(v3 n1, v3 n2, f32 t) {
	return (1.f - t) * n1 + t * n2;
}

v4 lerp(v4 n1, v4 n2, f32 t) {
	return (1.f - t) * n1 + t * n2;
}

v2 hadamard(v2 a, v2 b) {
	//Per component product
	v2 res;
	res.x = a.x * b.x;
	res.y = a.y * b.y;
	return res;
}

v3 hadamard(v3 a, v3 b) {
	//Per component product
	v3 res;
	res.x = a.x * b.x;
	res.y = a.y * b.y;
	res.z = a.z * b.z;
	return res;
}

v4 hadamard(v4 a, v4 b) {
	//Per component product
	v4 res;
	res.x = a.x * b.x;
	res.y = a.y * b.y;
	res.z = a.z * b.z;
	res.w = a.w * b.w;
	return res;
}

bool rcs_intersect(rc2 r1, rc2 r2) {
	v2 r1_min=r1.get_min(), r1_max=r1.get_max(), r2_min=r2.get_min(), r2_max=r2.get_max();
	bool res = !(r2_max.x <= r1_min.x || r2_min.x >= r1_max.x
		      || r2_max.y <= r1_min.y || r2_min.y >= r1_max.y);
	return res;
}

f32 minimum(f32 a, f32 b) {
	return a < b ? a : b;
}
f32 maximum(f32 a, f32 b) {
	return a > b ? a : b;
}

f32 safe_ratioN(f32 numerator, f32 divisor, f32 n) { //NOTE: interesting idea
	f32 res = n;
	if (divisor != 0.f) {
		res = numerator / divisor;
	}
	return res;
}

f32 safe_ratio0(f32 numerator, f32 divisor) { 
	return safe_ratioN(numerator,divisor,0.f);
}

f32 safe_ratio1(f32 numerator, f32 divisor) {
	return safe_ratioN(numerator,divisor,1.f);
}

f32 clamp(f32 min, f32 n, f32 max) {
	f32 res=n;
	if (res < min) res = min;
	else if (res > max) res = max;
	return res;
}

f32 clamp01(f32 n) {
	return clamp(0.f, n, 1.f);
}

i32 round_f32_to_i32(f32 n) {
	//TODO(fran): intrinsic
	i32 res = (i32)(n + .5f); //TODO(fran): does this work correctly with negative numbers?
	return res;
}

u32 round_f32_to_u32(f32 n) {
	game_assert(n >= 0.f);
	//TODO(fran): intrinsic
	u32 res = (u32)(n + .5f);
	return res;
}

v2 perp(v2 v) {//generate orthogonal vector
	v2 res; 
	res.x = -v.y;
	res.y = v.x;
	return res;
}

f32 squared(f32 n) {
	f32 res = n * n;
	return res;
}

f32 square_root(f32 n) {
	f32 res = sqrtf(n);
	return res;
}

f32 dot(v3 a, v3 b) {
	f32 res = a.x * b.x + a.y * b.y + a.z * b.z;
	return res;
}

f32 dot(v2 a, v2 b) {
	f32 res = a.x * b.x + a.y * b.y;
	return res;
}

f32 lenght_sq(v2 v) {
	f32 res;
	res = dot(v, v);
	return res;
}

f32 lenght_sq(v3 v) {
	f32 res;
	res = dot(v, v);
	return res;
}

f32 lenght(v3 v) {
	f32 res = sqrtf(lenght_sq(v));
	return res;
}

v3 normalize(v3 v) {
	v3 res = v / lenght(v); //TODO(fran): beware of division by zero
	return res;
}
