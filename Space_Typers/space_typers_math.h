#pragma once
#include <random>//TODO(fran):remove
#include <chrono>//TODO(fran):remove

f32 sign(f32 n) {
	f32 res;
	res = n >= 0.f ? 1.f : -1.f;
	return res;
}

f32 get_random_0_1() {
	//TODO(fran): xorshift + handamde day 434 / ray 02
	std::chrono::system_clock::rep seed = std::chrono::system_clock::now().time_since_epoch().count();
	static std::mt19937 generator((unsigned int)seed);
	std::uniform_real_distribution<> distribution{ 0.f,1.f };
	return (f32)distribution(generator);
}

f32 lerp(f32 n1, f32 n2, f32 t) { //TODO(fran): put t as the middle param?
	//NOTE: interesting that https://en.wikipedia.org/wiki/Linear_interpolation mentions this is the Precise method
	return (1 - t) * n1 + t * n2;
}

v2_f32 lerp(v2_f32 n1, v2_f32 n2, f32 t) {
	//NOTE: interesting that https://en.wikipedia.org/wiki/Linear_interpolation mentions this is the Precise method
	return (1 - t) * n1 + t * n2;
}

v2_f32 hadamard(v2_f32 v1, v2_f32 v2) {
	//Per component product
	v2_f32 res = { v1.x * v2.x, v1.y * v2.y };
	return res;
}

bool rcs_intersect(rc2 r1, rc2 r2) {
	v2_f32 r1_min=r1.get_min(), r1_max=r1.get_max(), r2_min=r2.get_min(), r2_max=r2.get_max();
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
	else if (res > min) res = max;
	return res;
}

f32 clamp01(f32 n) {
	return clamp(0.f, n, 1.f);
}