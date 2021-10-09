#include "affine2d.h"

Matrix<3, 3> scale(float x, float y)
{
    return Matrix<3, 3> {
        {x,    0.0f, 0.0f},
        {0.0f, y,    0.0f},
        {0.0f, 0.0f, 1.0f}
    };
}

Matrix<3, 3> translate(float x, float y)
{
    return Matrix<3, 3> {
        {1.0f, 0.0f, x   },
        {0.0f, 1.0f, y   },
        {0.0f, 0.0f, 1.0f}
    };
}
