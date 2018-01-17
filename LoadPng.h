#pragma once
#include <png.h>

bool loadPngImage(const char *name, int &outWidth, int &outHeight, bool &outHasAlpha, unsigned char **outData);
