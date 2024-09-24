#ifndef VECTOR_H
#define VECTOR_H

typedef struct Vec2i
{
	int x, y;
}Vec2i;

typedef struct Vec2f
{
	float x, y;
}Vec2f;

Vec2f		vec2f(float x, float y);
Vec2f		vec2f_add(Vec2f x, Vec2f y);
Vec2f		vec2f_sub(Vec2f x, Vec2f y);
Vec2f		vec2f_mul(Vec2f x, Vec2f y);
Vec2f		vec2f_div(Vec2f x, Vec2f y);

Vec2i		vec2i(int x, int y);
Vec2i		vec2i_add(Vec2i x, Vec2i y);
Vec2i		vec2i_sub(Vec2i x, Vec2i y);
Vec2i		vec2i_mul(Vec2i x, Vec2i y);
Vec2i		vec2i_div(Vec2i x, Vec2i y);

#endif // vector.h
