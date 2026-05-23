#ifndef _XCAT_COMPRESS_
#define _XCAT_COMPRESS_

#include "XCATPhantomCompress.hh"
#include <time.h>
#include <math.h>
#include <string.h>
#include <iostream>

using namespace catlib;
#include "FileIO.hh"

typedef unsigned char XCATmat_t;

void compare_values(unsigned char * phantom_buffer, unsigned long phantom_size, XCATPhantomCompress & compressor, tNode_t *root_node);
void speed_test(unsigned char * phantom_buffer, unsigned long phantom_size, XCATPhantomCompress & compressor, tNode_t *root_node);
int  write_rle_phantom_file(std::string phantom_file, unsigned char * r_buf, unsigned long r_buf_size);


#endif
