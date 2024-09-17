#include "vector.h"

Vec2f
vec2f(float a, float b)
{
	return (Vec2f){a, b};
}

Vec2f
vec2f_sub(Vec2f a, Vec2f b)
{
	return vec2f(a.x - b.x, a.y - b.y);
}

Vec2f
vec2f_add(Vec2f a, Vec2f b)
{
	return vec2f(a.x + b.x, a.y + b.y);
}

Vec2f
vec2f_mul(Vec2f a, Vec2f b)
{
	return vec2f(a.x * b.x, a.y * b.y);
}

Vec2f
vec2f_div(Vec2f a, Vec2f b)
{
	return vec2f(a.x / b.x, a.y / b.y);
}

Vec2i
vec2i(int a, int b)
{
	return (Vec2i){a, b};
}

Vec2i
vec2i_sub(Vec2i a, Vec2i b)
{
	return vec2i(a.x - b.x, a.y - b.y);
}

Vec2i
vec2i_add(Vec2i a, Vec2i b)
{
	return vec2i(a.x + b.x, a.y + b.y);
}

Vec2i
vec2i_mul(Vec2i a, Vec2i b)
{
	return vec2i(a.x * b.x, a.y * b.y);
}

Vec2i
vec2i_div(Vec2i a, Vec2i b)
{
	return vec2i(a.x / b.x, a.y / b.y);
}
