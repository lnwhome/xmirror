#include "LoadPng.h"
#include <iostream>
#include <string.h>
#include "TypesConf.h"

bool loadPngImage(const char *name, int &outWidth, int &outHeight, bool &outHasAlpha, unsigned char **outData)
{
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    int color_type, interlace_type;
    FILE *fp;
    
    if ((fp = fopen(name, "rb")) == NULL)
        return false;
    
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                     NULL, NULL, NULL);
    
    if (png_ptr == NULL) {
        fclose(fp);
        return false;
    }
    
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return false;
    }
    
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }
    
    png_init_io(png_ptr, fp);
    
    png_set_sig_bytes(png_ptr, sig_read);
    
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
    
    png_uint_32 width, height;
    int bit_depth;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);
    logd_ << "png: " << width << " " << height << " " << bit_depth << " " << color_type << "\n";
    outHasAlpha = (color_type == PNG_COLOR_TYPE_RGB_ALPHA);
    outWidth = width;
    outHeight = height;
    
    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    logd_ << "png allocated " << row_bytes * outHeight << " bytes\n";
    *outData = (unsigned char*) malloc(row_bytes * outHeight);
    
    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
    
    for (int i = 0; i < outHeight; i++) {
        memcpy(*outData+(row_bytes * (outHeight-1-i)), row_pointers[i], row_bytes);
    }
    for(unsigned int pp = 0;pp < row_bytes*outHeight; pp+=3)
    {
       (*outData)[pp+0] = (*outData)[pp+0];//r
       (*outData)[pp+1] = (*outData)[pp+1];//g
       (*outData)[pp+2] = (*outData)[pp+2];//b
    }
   
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    
    fclose(fp);
    
    return true;
}


