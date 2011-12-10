/*
 * Copyright (C) 2011 The HTC-Linux Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/StopWatch.h>

#include <hardware/hardware.h>

#include <linux/android_pmem.h>

#include <GLES/gl.h>

#include "OGLAlloc.h"
#include "clz.h"

namespace android {

// ---------------------------------------------------------------------------

static void checkGLErrors()
{
    do {
        // there could be more than one error flag
        GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        LOGE("GL error 0x%04x", int(error));
    } while(true);
}

static unsigned int* gpu1_base=0;
static int textures_allocated=0;
static int createGLTexture(int w, int h, int format, GLuint *text, int *stride, int *size)
{
    int i, j, base;

    int bpp = 0;
    GLuint components = GL_RGBA;
    GLuint data_format = GL_UNSIGNED_BYTE;

    //init_texture_access();
 /*   if(!gpu1_base)
    {
       	LOGE("GPU not properly initialized during memory allocation");
  	return -1;
    }*/

    LOGE("createGLTexture: 1");

    textures_allocated++;

    switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_RGB_565:
	    components = GL_RGB;
            data_format = GL_UNSIGNED_SHORT_5_6_5;
            bpp = 2;
	    break;
        case HAL_PIXEL_FORMAT_RGBA_4444:
	    data_format = GL_UNSIGNED_SHORT_4_4_4_4;
            bpp = 2;
            break;
        default:
            return -EINVAL;
    }

    LOGE("createGLTexture: 2");

    *stride = w;
    *size = w * h * bpp;
    
    LOGE("createGLTexture: 3");

    glEnable (GL_TEXTURE_2D);
    checkGLErrors();
    glGenTextures(1,text);
    checkGLErrors();
    glBindTexture(GL_TEXTURE_2D, *text);
    checkGLErrors();

    unsigned int* data = (unsigned int*)malloc(w*h*bpp);
    for(i=0; i < w*h*bpp/4; i++)
    {
       data[i] = rand();
    }

    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    checkGLErrors();

    glTexImage2D(GL_TEXTURE_2D, 0,
                    components, w, h, 0, 
		    components, data_format, data);
    checkGLErrors();

    //We have to use the texture of the blob will not actually upload it to 
    //Gpu memory
    //TODO: make sure this drawing takes place in some small corner of the display
    //or potentially out of the viewport all together. 
    GLfloat vertices[] = {50,50, 75,50, 50,75, 75,75};
    GLfloat texcoords[] = {0,0, 0,1, 1,1, 1,0};

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    //TODO: make sure this was really disabled when we started
    //also check that VERTEX_ARRAY was already on
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glFinish();
    checkGLErrors();

    //TODO: get gpu1 size from somewhere
    //Lets find it
    base=0;
    for(i=0x2F0000/4; i < (13*1024*1024-w*h*bpp)/4; i++)
    {
	base=i;
 	for(j=0; j < w*h*bpp/4; j++)
	{
	    if(gpu1_base[i+j] != data[j])
	    {
		if(textures_allocated==1)
			LOGE("Got 0x%8.8X Expected 0x%8.8X at 0x%8.8X", gpu1_base[i+j], data[j], i);
		base=0;
		break;
            }
	}
	if(base||textures_allocated==1)
	    break;
    }

    if(base)
	LOGW("Located texture at 0x%8.8X", base*4);
    else
	LOGE("Failed to locate texture");

    free(data);
    return base*4;
}


void* OGLAlloc::Alloc(int w, int h, int format, GLuint *text, int* stride, int* size, void* base)
{
	gpu1_base = (unsigned int*)base;
	//TODO: check if we can reuse part of an already allocated texture

        // find the smallest power-of-two that will accommodate our surface
        int potWidth  = 1 << (31 - clz(w));
        int potHeight = 1 << (31 - clz(h));
        if (potWidth  < w) potWidth  <<= 1;
        if (potHeight < h) potHeight <<= 1;

	int loc = createGLTexture(potWidth,potHeight,format,text,stride,size);
  //      glDeleteTextures(1,text);

	return (void*)loc;
}

void OGLAlloc::Free(GLuint text)
{
	glDeleteTextures(1,&text);
}

// ---------------------------------------------------------------------------

}; // namespace android
