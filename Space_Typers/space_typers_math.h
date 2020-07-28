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

f32 lerp(f32 n1, f32 n2, f32 t) {
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
