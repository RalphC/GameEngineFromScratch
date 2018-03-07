#pragma once

#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif // !PI

#ifndef TWO_PI
#define TWO_PI 3.14159265358979323846f * 2.0f
#endif // !TWO_PI

typedef struct VectorType {
	union {
		struct { float x, y, z; };
		struct { float r, g, b; };
	};
}VectorType;

void MatrixRotationYawPitchRoll(float* matrix, float yaw, float pitch, float roll)
{

}