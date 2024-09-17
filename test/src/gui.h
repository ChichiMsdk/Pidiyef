#ifndef GUI_H
#define GUI_H

#define mEaseInOutQuad(t) \
	((t) < 0.5f ? (2.0f * (t) * (t)) : (-1.0f + (4.0f - 2.0f * (t)) * (t)))

#define mInterpolate2(start, end, factor) \
	((start) + mEaseInOutQuad(factor) * ((end) - (start)))

#define mInterpolate(start, end, factor) \
	((start) + (factor) * ((end) - (start)))

#endif
