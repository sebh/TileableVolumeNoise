
/**************************************************************************
 ** Simplified TARGA library for Intro to Graphics Classes
 **
 ** This is a simple library for reading and writing image files in
 ** the TARGA file format (which is a simple format). 
 ** The routines are intentionally designed to be simple for use in
 ** into to graphics assignments - a more full-featured targa library
 ** also exists for other uses.
 **
 ** This library was originally written by Alex Mohr who has assigned
 ** copyright to Michael Gleicher. The code is made available under an
 ** "MIT" Open Source license.
 **/

/**
 ** Copyright (c) 2005 Michael L. Gleicher
 **
 ** Permission is hereby granted, free of charge, to any person
 ** obtaining a copy of this software and associated documentation
 ** files (the "Software"), to deal in the Software without
 ** restriction, including without limitation the rights to use, copy,
 ** modify, merge, publish, distribute, sublicense, and/or sell copies
 ** of the Software, and to permit persons to whom the Software is
 ** furnished to do so, subject to the following conditions:
 ** 
 ** The above copyright notice and this permission notice shall be
 ** included in all copies or substantial portions of the Software.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 ** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 ** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 ** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 ** HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 ** WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 ** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 ** DEALINGS IN THE SOFTWARE.
 **/

/*
** libtarga.c -- routines for reading targa files.
*/

/*
  Modified by yu-chi because of initialization of variables at tga_load
  09-16-2005
*/

#include <stdio.h>
#include <malloc.h>

#include "libtarga.h"




#define TGA_IMG_NODATA             (0)
#define TGA_IMG_UNC_PALETTED       (1)
#define TGA_IMG_UNC_TRUECOLOR      (2)
#define TGA_IMG_UNC_GRAYSCALE      (3)
#define TGA_IMG_RLE_PALETTED       (9)
#define TGA_IMG_RLE_TRUECOLOR      (10)
#define TGA_IMG_RLE_GRAYSCALE      (11)


#define TGA_LOWER_LEFT             (0)
#define TGA_LOWER_RIGHT            (1)
#define TGA_UPPER_LEFT             (2)
#define TGA_UPPER_RIGHT            (3)


#define HDR_LENGTH               (18)
#define HDR_IDLEN                (0)
#define HDR_CMAP_TYPE            (1)
#define HDR_IMAGE_TYPE           (2)
#define HDR_CMAP_FIRST           (3)
#define HDR_CMAP_LENGTH          (5)
#define HDR_CMAP_ENTRY_SIZE      (7)
#define HDR_IMG_SPEC_XORIGIN     (8)
#define HDR_IMG_SPEC_YORIGIN     (10)
#define HDR_IMG_SPEC_WIDTH       (12)
#define HDR_IMG_SPEC_HEIGHT      (14)
#define HDR_IMG_SPEC_PIX_DEPTH   (16)
#define HDR_IMG_SPEC_IMG_DESC    (17)



#define TGA_ERR_NONE                    (0)
#define TGA_ERR_BAD_HEADER              (1)
#define TGA_ERR_OPEN_FAILS              (2)
#define TGA_ERR_BAD_FORMAT              (3)
#define TGA_ERR_UNEXPECTED_EOF          (4)
#define TGA_ERR_NODATA_IMAGE            (5)
#define TGA_ERR_COLORMAP_FOR_GRAY       (6)
#define TGA_ERR_BAD_COLORMAP_ENTRY_SIZE (7)
#define TGA_ERR_BAD_COLORMAP            (8)
#define TGA_ERR_READ_FAILS              (9)
#define TGA_ERR_BAD_IMAGE_TYPE          (10)
#define TGA_ERR_BAD_DIMENSIONS          (11)



static uint32 TargaError;


static int16 ttohs( int16 val );
static int16 htots( int16 val );
static int32 ttohl( int32 val );
static int32 htotl( int32 val );


static uint32 tga_get_pixel( FILE * tga, ubyte bytes_per_pix, 
                            ubyte * colormap, ubyte cmap_bytes_entry );
static uint32 tga_convert_color( uint32 pixel, uint32 bpp_in, ubyte alphabits, uint32 format_out );
static void tga_write_pixel_to_mem( ubyte * dat, ubyte img_spec, uint32 number, 
                                   uint32 w, uint32 h, uint32 pixel, uint32 format );



/* returns the last error encountered */
int tga_get_last_error() {
    return( TargaError );
}


/* returns a pointer to the string for an error code */
const char * tga_error_string( int error_code ) {

    switch( error_code ) {
    
    case TGA_ERR_NONE:
        return( "no error" );

    case TGA_ERR_BAD_HEADER:
        return( "bad image header" );

    case TGA_ERR_OPEN_FAILS:
        return( "cannot open file" );

    case TGA_ERR_BAD_FORMAT:
        return( "bad format argument" );

    case TGA_ERR_UNEXPECTED_EOF:
        return( "unexpected end-of-file" );

    case TGA_ERR_NODATA_IMAGE:
        return( "image contains no data" );

    case TGA_ERR_COLORMAP_FOR_GRAY:
        return( "found colormap for a grayscale image" );

    case TGA_ERR_BAD_COLORMAP_ENTRY_SIZE:
        return( "unsupported colormap entry size" );

    case TGA_ERR_BAD_COLORMAP:
        return( "bad colormap" );

    case TGA_ERR_READ_FAILS:
        return( "cannot read from file" );

    case TGA_ERR_BAD_IMAGE_TYPE:
        return( "unknown image type" );

    case TGA_ERR_BAD_DIMENSIONS:
        return( "image has size 0 width or height (or both)" );

    default:
        return( "unknown error" );

    }

    // shut up compiler..
    return( NULL );

}



/* creates a targa image of the desired format */
void * tga_create( int width, int height, unsigned int format ) {

    switch( format ) {
        
    case TGA_TRUECOLOR_32:
        return( (void *)malloc( width * height * 4 ) );
        
    case TGA_TRUECOLOR_24:
        return( (void *)malloc( width * height * 3 ) );
        
    default:
        TargaError = TGA_ERR_BAD_FORMAT;
        break;

    }

    return( NULL );

}



/* loads and converts a targa from disk */
void * tga_load( const char * filename, 
                int * width, int * height, unsigned int format ) {
    
    ubyte  idlen;               // length of the image_id string below.
    ubyte  cmap_type;           // paletted image <=> cmap_type
    ubyte  image_type;          // can be any of the IMG_TYPE constants above.
    uint16 cmap_first;          // 
    uint16 cmap_length;         // how long the colormap is
    ubyte  cmap_entry_size;     // how big a palette entry is.
    uint16 img_spec_xorig;      // the x origin of the image in the image data.
    uint16 img_spec_yorig;      // the y origin of the image in the image data.
    uint16 img_spec_width;      // the width of the image.
    uint16 img_spec_height;     // the height of the image.
    ubyte  img_spec_pix_depth;  // the depth of a pixel in the image.
    ubyte  img_spec_img_desc;   // the image descriptor.

    FILE * targafile;

    ubyte * tga_hdr = NULL;

    ubyte * colormap = NULL;

    //***********************************************************************
    // Add by Yu-Chi because of variable initialization.
    // Add all = 0 to all the following variables
    //***********************************************************************


    ubyte cmap_bytes_entry = 0;
    uint32 cmap_bytes = 0;
    
    uint32 tmp_col = 0;
    uint32 tmp_int32 = 0;
    ubyte  tmp_byte = 0;

    ubyte alphabits = 0;

    uint32 num_pixels = 0;
    
    uint32 i = 0;
    uint32 j = 0;

    ubyte * image_data = 0;
    uint32 img_dat_len = 0;

    ubyte bytes_per_pix = 0;

    ubyte true_bits_per_pixel = 0;

    uint32 bytes_total = 0;

    ubyte packet_header = 0;
    ubyte repcount = 0;
    

    switch( format ) {

    case TGA_TRUECOLOR_24:
    case TGA_TRUECOLOR_32:
        break;

    default:
        TargaError = TGA_ERR_BAD_FORMAT;
        return( NULL );

    }

    
    /* open binary image file */
    targafile = fopen( filename, "rb" );
    if( targafile == NULL ) {
        TargaError = TGA_ERR_OPEN_FAILS;
        return( NULL );
    }


    /* allocate memory for the header */
    tga_hdr = (ubyte *)malloc( HDR_LENGTH );

    /* read the header in. */
    if( fread( (void *)tga_hdr, 1, HDR_LENGTH, targafile ) != HDR_LENGTH ) {
        free( tga_hdr );
        TargaError = TGA_ERR_BAD_HEADER;
        return( NULL );
    }

    
    /* byte order is important here. */
    idlen              = (ubyte)tga_hdr[HDR_IDLEN];
    
    image_type         = (ubyte)tga_hdr[HDR_IMAGE_TYPE];
    
    cmap_type          = (ubyte)tga_hdr[HDR_CMAP_TYPE];
    cmap_first         = ttohs( *(uint16 *)(&tga_hdr[HDR_CMAP_FIRST]) );
    cmap_length        = ttohs( *(uint16 *)(&tga_hdr[HDR_CMAP_LENGTH]) );
    cmap_entry_size    = (ubyte)tga_hdr[HDR_CMAP_ENTRY_SIZE];

    img_spec_xorig     = ttohs( *(uint16 *)(&tga_hdr[HDR_IMG_SPEC_XORIGIN]) );
    img_spec_yorig     = ttohs( *(uint16 *)(&tga_hdr[HDR_IMG_SPEC_YORIGIN]) );
    img_spec_width     = ttohs( *(uint16 *)(&tga_hdr[HDR_IMG_SPEC_WIDTH]) );
    img_spec_height    = ttohs( *(uint16 *)(&tga_hdr[HDR_IMG_SPEC_HEIGHT]) );
    img_spec_pix_depth = (ubyte)tga_hdr[HDR_IMG_SPEC_PIX_DEPTH];
    img_spec_img_desc  = (ubyte)tga_hdr[HDR_IMG_SPEC_IMG_DESC];

    free( tga_hdr );


    num_pixels = img_spec_width * img_spec_height;

    if( num_pixels == 0 ) {
        TargaError = TGA_ERR_BAD_DIMENSIONS;
        return( NULL );
    }

    
    alphabits = img_spec_img_desc & 0x0F;

    
    /* seek past the image id, if there is one */
    if( idlen ) {
        if( fseek( targafile, idlen, SEEK_CUR ) ) {
            TargaError = TGA_ERR_UNEXPECTED_EOF;
            return( NULL );
        }
    }


    /* if this is a 'nodata' image, just jump out. */
    if( image_type == TGA_IMG_NODATA ) {
        TargaError = TGA_ERR_NODATA_IMAGE;
        return( NULL );
    }


    /* now we're starting to get into the meat of the matter. */
    
    
    /* deal with the colormap, if there is one. */
    if( cmap_type ) {

        switch( image_type ) {
            
        case TGA_IMG_UNC_PALETTED:
        case TGA_IMG_RLE_PALETTED:
            break;
            
        case TGA_IMG_UNC_TRUECOLOR:
        case TGA_IMG_RLE_TRUECOLOR:
            // this should really be an error, but some really old
            // crusty targas might actually be like this (created by TrueVision, no less!)
            // so, we'll hack our way through it.
            break;
            
        case TGA_IMG_UNC_GRAYSCALE:
        case TGA_IMG_RLE_GRAYSCALE:
            TargaError = TGA_ERR_COLORMAP_FOR_GRAY;
            return( NULL );
        }
        
        /* ensure colormap entry size is something we support */
        if( !(cmap_entry_size == 15 || 
            cmap_entry_size == 16 ||
            cmap_entry_size == 24 ||
            cmap_entry_size == 32) ) {
            TargaError = TGA_ERR_BAD_COLORMAP_ENTRY_SIZE;
            return( NULL );
        }
        
        
        /* allocate memory for a colormap */
        if( cmap_entry_size & 0x07 ) {
            cmap_bytes_entry = (((8 - (cmap_entry_size & 0x07)) + cmap_entry_size) >> 3);
        } else {
            cmap_bytes_entry = (cmap_entry_size >> 3);
        }
        
        cmap_bytes = cmap_bytes_entry * cmap_length;
        colormap = (ubyte *)malloc( cmap_bytes );
        
        
        for( i = 0; i < cmap_length; i++ ) {
            
            /* seek ahead to first entry used */
            if( cmap_first != 0 ) {
                fseek( targafile, cmap_first * cmap_bytes_entry, SEEK_CUR );
            }
            
            tmp_int32 = 0;
            for( j = 0; j < cmap_bytes_entry; j++ ) {
                if( !fread( &tmp_byte, 1, 1, targafile ) ) {
                    free( colormap );
                    TargaError = TGA_ERR_BAD_COLORMAP;
                    return( NULL );
                }
                tmp_int32 += tmp_byte << (j * 8);
            }

            // byte order correct.
            tmp_int32 = ttohl( tmp_int32 );

            for( j = 0; j < cmap_bytes_entry; j++ ) {
                colormap[i * cmap_bytes_entry + j] = (tmp_int32 >> (8 * j)) & 0xFF;
            }
            
        }

    }


    // compute number of bytes in an image data unit (either index or BGR triple)
    if( img_spec_pix_depth & 0x07 ) {
        bytes_per_pix = (((8 - (img_spec_pix_depth & 0x07)) + img_spec_pix_depth) >> 3);
    } else {
        bytes_per_pix = (img_spec_pix_depth >> 3);
    }


    /* assume that there's one byte per pixel */
    if( bytes_per_pix == 0 ) {
        bytes_per_pix = 1;
    }


    /* compute how many bytes of storage we need for the image */
    bytes_total = img_spec_width * img_spec_height * format;

    image_data = (ubyte *)malloc( bytes_total );

    img_dat_len = img_spec_width * img_spec_height * bytes_per_pix;

    // compute the true number of bits per pixel
    true_bits_per_pixel = cmap_type ? cmap_entry_size : img_spec_pix_depth;

    switch( image_type ) {

    case TGA_IMG_UNC_TRUECOLOR:
    case TGA_IMG_UNC_GRAYSCALE:
    case TGA_IMG_UNC_PALETTED:

        /* FIXME: support grayscale */

        for( i = 0; i < num_pixels; i++ ) {

            // get the color value.
            tmp_col = tga_get_pixel( targafile, bytes_per_pix, colormap, cmap_bytes_entry );
            tmp_col = tga_convert_color( tmp_col, true_bits_per_pixel, alphabits, format );
            
            // now write the data out.
            tga_write_pixel_to_mem( image_data, img_spec_img_desc, 
                i, img_spec_width, img_spec_height, tmp_col, format );

        }
    
        break;


    case TGA_IMG_RLE_TRUECOLOR:
    case TGA_IMG_RLE_GRAYSCALE:
    case TGA_IMG_RLE_PALETTED:

        // FIXME: handle grayscale..

        for( i = 0; i < num_pixels; ) {

            /* a bit of work to do to read the data.. */
            if( fread( &packet_header, 1, 1, targafile ) < 1 ) {
                // well, just let them fill the rest with null pixels then...
                packet_header = 1;
            }

            if( packet_header & 0x80 ) {
                /* run length packet */

                tmp_col = tga_get_pixel( targafile, bytes_per_pix, colormap, cmap_bytes_entry );
                tmp_col = tga_convert_color( tmp_col, true_bits_per_pixel, alphabits, format );
                
                repcount = (packet_header & 0x7F) + 1;
                
                /* write all the data out */
                for( j = 0; j < repcount; j++ ) {
                    tga_write_pixel_to_mem( image_data, img_spec_img_desc, 
                        i + j, img_spec_width, img_spec_height, tmp_col, format );
                }

                i += repcount;

            } else {
                /* raw packet */
                /* get pixel from file */
                
                repcount = (packet_header & 0x7F) + 1;
                
                for( j = 0; j < repcount; j++ ) {
                    
                    tmp_col = tga_get_pixel( targafile, bytes_per_pix, colormap, cmap_bytes_entry );
                    tmp_col = tga_convert_color( tmp_col, true_bits_per_pixel, alphabits, format );
                    
                    tga_write_pixel_to_mem( image_data, img_spec_img_desc, 
                        i + j, img_spec_width, img_spec_height, tmp_col, format );

                }

                i += repcount;

            }

        }

        break;
    

    default:

        TargaError = TGA_ERR_BAD_IMAGE_TYPE;
        return( NULL );

    }

    fclose( targafile );

    *width  = img_spec_width;
    *height = img_spec_height;

    return( (void *)image_data );

}





int tga_write_raw( const char * file, int width, int height, unsigned char * dat, unsigned int format ) {

    FILE * tga;

    uint32 i, j;

    uint32 size = width * height;

    float red, green, blue, alpha;

    char id[] = "written with libtarga";
    ubyte idlen = 21;
    ubyte zeroes[5] = { 0, 0, 0, 0, 0 };
    uint32 pixbuf;
    ubyte one = 1;
    ubyte cmap_type = 0;
    ubyte img_type  = 2;  // 2 - uncompressed truecolor  10 - RLE truecolor
    uint16 xorigin  = 0;
    uint16 yorigin  = 0;
    ubyte  pixdepth = format * 8;  // bpp
    ubyte img_desc;
    
    
    switch( format ) {

    case TGA_TRUECOLOR_24:
        img_desc = 0;
        break;

    case TGA_TRUECOLOR_32:
        img_desc = 8;
        break;

    default:
        TargaError = TGA_ERR_BAD_FORMAT;
        return( 0 );
        break;

    }

    tga = fopen( file, "wb" );

    if( tga == NULL ) {
        TargaError = TGA_ERR_OPEN_FAILS;
        return( 0 );
    }

    // write id length
    fwrite( &idlen, 1, 1, tga );

    // write colormap type
    fwrite( &cmap_type, 1, 1, tga );

    // write image type
    fwrite( &img_type, 1, 1, tga );

    // write cmap spec.
    fwrite( &zeroes, 5, 1, tga );

    // write image spec.
    fwrite( &xorigin, 2, 1, tga );
    fwrite( &yorigin, 2, 1, tga );
    fwrite( &width, 2, 1, tga );
    fwrite( &height, 2, 1, tga );
    fwrite( &pixdepth, 1, 1, tga );
    fwrite( &img_desc, 1, 1, tga );


    // write image id.
    fwrite( &id, idlen, 1, tga );

    // color correction -- data is in RGB, need BGR.
    for( i = 0; i < size; i++ ) {

        pixbuf = 0;
        for( j = 0; j < format; j++ ) {
            pixbuf += dat[i*format+j] << (8 * j);
        }

        switch( format ) {

        case TGA_TRUECOLOR_24:

            pixbuf = ((pixbuf & 0xFF) << 16) + 
                     (pixbuf & 0xFF00) + 
                     ((pixbuf & 0xFF0000) >> 16);

            pixbuf = htotl( pixbuf );
            
            fwrite( &pixbuf, 3, 1, tga );

            break;

        case TGA_TRUECOLOR_32:

            /* need to un-premultiply alpha.. */

            red     = (pixbuf & 0xFF) / 255.0f;
            green   = ((pixbuf & 0xFF00) >> 8) / 255.0f;
            blue    = ((pixbuf & 0xFF0000) >> 16) / 255.0f;
            alpha   = ((pixbuf & 0xFF000000) >> 24) / 255.0f;

            if( alpha > 0.0001 ) {
                red /= alpha;
                green /= alpha;
                blue /= alpha;
            }

            /* clamp to 1.0f */

            red = red > 1.0f ? 255.0f : red * 255.0f;
            green = green > 1.0f ? 255.0f : green * 255.0f;
            blue = blue > 1.0f ? 255.0f : blue * 255.0f;
            alpha = alpha > 1.0f ? 255.0f : alpha * 255.0f;

            pixbuf = (ubyte)blue + (((ubyte)green) << 8) + 
                (((ubyte)red) << 16) + (((ubyte)alpha) << 24);
                
            pixbuf = htotl( pixbuf );
           
            fwrite( &pixbuf, 4, 1, tga );

            break;

        }

    }

    fclose( tga );

    return( 1 );

}





int tga_write_rle( const char * file, int width, int height, unsigned char * dat, unsigned int format ) {

    FILE * tga;

    uint32 i, j;
    uint32 oc, nc;

    enum RLE_STATE { INIT, NONE, RLP, RAWP };

    int state = INIT;

    uint32 size = width * height;

    uint16 shortwidth = (uint16)width;
    uint16 shortheight = (uint16)height;

    ubyte repcount;

    float red, green, blue, alpha;

    int idx, row, column;

    // have to buffer a whole line for raw packets.
    unsigned char * rawbuf = (unsigned char *)malloc( width * format );  

    char id[] = "written with libtarga";
    ubyte idlen = 21;
    ubyte zeroes[5] = { 0, 0, 0, 0, 0 };
    uint32 pixbuf;
    ubyte one = 1;
    ubyte cmap_type = 0;
    ubyte img_type  = 10;  // 2 - uncompressed truecolor  10 - RLE truecolor
    uint16 xorigin  = 0;
    uint16 yorigin  = 0;
    ubyte  pixdepth = format * 8;  // bpp
    ubyte img_desc  = format == TGA_TRUECOLOR_32 ? 8 : 0;
  

    switch( format ) {
    case TGA_TRUECOLOR_24:
    case TGA_TRUECOLOR_32:
        break;

    default:
        TargaError = TGA_ERR_BAD_FORMAT;
        return( 0 );
    }


    tga = fopen( file, "wb" );

    if( tga == NULL ) {
        TargaError = TGA_ERR_OPEN_FAILS;
        return( 0 );
    }

    // write id length
    fwrite( &idlen, 1, 1, tga );

    // write colormap type
    fwrite( &cmap_type, 1, 1, tga );

    // write image type
    fwrite( &img_type, 1, 1, tga );

    // write cmap spec.
    fwrite( &zeroes, 5, 1, tga );

    // write image spec.
    fwrite( &xorigin, 2, 1, tga );
    fwrite( &yorigin, 2, 1, tga );
    fwrite( &shortwidth, 2, 1, tga );
    fwrite( &shortheight, 2, 1, tga );
    fwrite( &pixdepth, 1, 1, tga );
    fwrite( &img_desc, 1, 1, tga );


    // write image id.
    fwrite( &id, idlen, 1, tga );

    // initial color values -- just to shut up the compiler.
    nc = 0;

    // color correction -- data is in RGB, need BGR.
    // also run-length-encoding.
    for( i = 0; i < size; i++ ) {

        idx = i * format;

        row = i / width;
        column = i % width;

        //printf( "row: %d, col: %d\n", row, column );
        pixbuf = 0;
        for( j = 0; j < format; j++ ) {
            pixbuf += dat[idx+j] << (8 * j);
        }

        switch( format ) {

        case TGA_TRUECOLOR_24:

            pixbuf = ((pixbuf & 0xFF) << 16) + 
                     (pixbuf & 0xFF00) + 
                     ((pixbuf & 0xFF0000) >> 16);

            pixbuf = htotl( pixbuf );
            break;

        case TGA_TRUECOLOR_32:

            /* need to un-premultiply alpha.. */

            red     = (pixbuf & 0xFF) / 255.0f;
            green   = ((pixbuf & 0xFF00) >> 8) / 255.0f;
            blue    = ((pixbuf & 0xFF0000) >> 16) / 255.0f;
            alpha   = ((pixbuf & 0xFF000000) >> 24) / 255.0f;

            if( alpha > 0.0001 ) {
                red /= alpha;
                green /= alpha;
                blue /= alpha;
            }

            /* clamp to 1.0f */

            red = red > 1.0f ? 255.0f : red * 255.0f;
            green = green > 1.0f ? 255.0f : green * 255.0f;
            blue = blue > 1.0f ? 255.0f : blue * 255.0f;
            alpha = alpha > 1.0f ? 255.0f : alpha * 255.0f;

            pixbuf = (ubyte)blue + (((ubyte)green) << 8) + 
                (((ubyte)red) << 16) + (((ubyte)alpha) << 24);
                
            pixbuf = htotl( pixbuf );
            break;

        }


        oc = nc;

        nc = pixbuf;


        switch( state ) {

        case INIT:
            // this is just used to make sure we have 2 pixel values to consider.
            state = NONE;
            break;


        case NONE:

            if( column == 0 ) {
                // write a 1 pixel raw packet for the old pixel, then go thru again.
                repcount = 0;
                fwrite( &repcount, 1, 1, tga );
#ifdef WORDS_BIGENDIAN
                fwrite( (&oc)+4, format, 1, tga );  // byte order..
#else
                fwrite( &oc, format, 1, tga );
#endif
                state = NONE;
                break;
            }

            if( nc == oc ) {
                repcount = 0;
                state = RLP;
            } else {
                repcount = 0;
                state = RAWP;
                for( j = 0; j < format; j++ ) {
#ifdef WORDS_BIGENDIAN
                    rawbuf[(repcount * format) + j] = (ubyte)(*((&oc)+format-j-1));
#else
                    rawbuf[(repcount * format) + j] = *(((ubyte *)(&oc)) + j);
#endif
                }
            }
            break;


        case RLP:
            repcount++;

            if( column == 0 ) {
                // finish off rlp.
                repcount |= 0x80;
                fwrite( &repcount, 1, 1, tga );
#ifdef WORDS_BIGENDIAN
                fwrite( (&oc)+4, format, 1, tga );  // byte order..
#else
                fwrite( &oc, format, 1, tga );
#endif
                state = NONE;
                break;
            }

            if( repcount == 127 ) {
                // finish off rlp.
                repcount |= 0x80;
                fwrite( &repcount, 1, 1, tga );
#ifdef WORDS_BIGENDIAN
                fwrite( (&oc)+4, format, 1, tga );  // byte order..
#else
                fwrite( &oc, format, 1, tga );
#endif
                state = NONE;
                break;
            }

            if( nc != oc ) {
                // finish off rlp
                repcount |= 0x80;
                fwrite( &repcount, 1, 1, tga );
#ifdef WORDS_BIGENDIAN
                fwrite( (&oc)+4, format, 1, tga );  // byte order..
#else
                fwrite( &oc, format, 1, tga );
#endif
                state = NONE;
            }
            break;


        case RAWP:
            repcount++;

            if( column == 0 ) {
                // finish off rawp.
                for( j = 0; j < format; j++ ) {
#ifdef WORDS_BIGENDIAN
                    rawbuf[(repcount * format) + j] = (ubyte)(*((&oc)+format-j-1));
#else
                    rawbuf[(repcount * format) + j] = *(((ubyte *)(&oc)) + j);
#endif
                }
                fwrite( &repcount, 1, 1, tga );
                fwrite( rawbuf, (repcount + 1) * format, 1, tga );
                state = NONE;
                break;
            }

            if( repcount == 127 ) {
                // finish off rawp.
                for( j = 0; j < format; j++ ) {
#ifdef WORDS_BIGENDIAN
                    rawbuf[(repcount * format) + j] = (ubyte)(*((&oc)+format-j-1));
#else
                    rawbuf[(repcount * format) + j] = *(((ubyte *)(&oc)) + j);
#endif
                }
                fwrite( &repcount, 1, 1, tga );
                fwrite( rawbuf, (repcount + 1) * format, 1, tga );
                state = NONE;
                break;
            }

            if( nc == oc ) {
                // finish off rawp
                repcount--;
                fwrite( &repcount, 1, 1, tga );
                fwrite( rawbuf, (repcount + 1) * format, 1, tga );
                
                // start new rlp
                repcount = 0;
                state = RLP;
                break;
            }

            // continue making rawp
            for( j = 0; j < format; j++ ) {
#ifdef WORDS_BIGENDIAN
                rawbuf[(repcount * format) + j] = (ubyte)(*((&oc)+format-j-1));
#else
                rawbuf[(repcount * format) + j] = *(((ubyte *)(&oc)) + j);
#endif
            }

            break;

        }
       

    }


    // clean up state.

    switch( state ) {

    case INIT:
        break;

    case NONE:
        // write the last 2 pixels in a raw packet.
        fwrite( &one, 1, 1, tga );
#ifdef WORDS_BIGENDIAN
                fwrite( (&oc)+4, format, 1, tga );  // byte order..
#else
                fwrite( &oc, format, 1, tga );
#endif
#ifdef WORDS_BIGENDIAN
                fwrite( (&nc)+4, format, 1, tga );  // byte order..
#else
                fwrite( &nc, format, 1, tga );
#endif
        break;

    case RLP:
        repcount++;
        repcount |= 0x80;
        fwrite( &repcount, 1, 1, tga );
#ifdef WORDS_BIGENDIAN
                fwrite( (&oc)+4, format, 1, tga );  // byte order..
#else
                fwrite( &oc, format, 1, tga );
#endif
        break;

    case RAWP:
        repcount++;
        for( j = 0; j < format; j++ ) {
#ifdef WORDS_BIGENDIAN
            rawbuf[(repcount * format) + j] = (ubyte)(*((&oc)+format-j-1));
#else
            rawbuf[(repcount * format) + j] = *(((ubyte *)(&oc)) + j);
#endif
        }
        fwrite( &repcount, 1, 1, tga );
        fwrite( rawbuf, (repcount + 1) * 3, 1, tga );
        break;

    }


    // close the file.
    fclose( tga );

    free( rawbuf );

    return( 1 );

}






/*************************************************************************************************/







static void tga_write_pixel_to_mem( ubyte * dat, ubyte img_spec, uint32 number, 
                                   uint32 w, uint32 h, uint32 pixel, uint32 format ) {

    // write the pixel to the data regarding how the
    // header says the data is ordered.

    uint32 j;
    uint32 x, y;
    uint32 addy;

    switch( (img_spec & 0x30) >> 4 ) {

    case TGA_LOWER_RIGHT:
        x = w - 1 - (number % w);
        y = number / h;
        break;

    case TGA_UPPER_LEFT:
        x = number % w;
        y = h - 1 - (number / w);
        break;

    case TGA_UPPER_RIGHT:
        x = w - 1 - (number % w);
        y = h - 1 - (number / w);
        break;

    case TGA_LOWER_LEFT:
    default:
        x = number % w;
        y = number / w;
        break;

    }

    addy = (y * w + x) * format;
    for( j = 0; j < format; j++ ) {
        dat[addy + j] = (ubyte)((pixel >> (j * 8)) & 0xFF);
    }
    
}





static uint32 tga_get_pixel( FILE * tga, ubyte bytes_per_pix, 
                            ubyte * colormap, ubyte cmap_bytes_entry ) {
    
    /* get the image data value out */

    uint32 tmp_col;
    uint32 tmp_int32;
    ubyte tmp_byte;

    uint32 j;

    tmp_int32 = 0;
    for( j = 0; j < bytes_per_pix; j++ ) {
        if( fread( &tmp_byte, 1, 1, tga ) < 1 ) {
            tmp_int32 = 0;
        } else {
            tmp_int32 += tmp_byte << (j * 8);
        }
    }
    
    /* byte-order correct the thing */
    switch( bytes_per_pix ) {
        
    case 2:
        tmp_int32 = ttohs( (uint16)tmp_int32 );
        break;
        
    case 3: /* intentional fall-thru */
    case 4:
        tmp_int32 = ttohl( tmp_int32 );
        break;
        
    }
    
    if( colormap != NULL ) {
        /* need to look up value to get real color */
        tmp_col = 0;
        for( j = 0; j < cmap_bytes_entry; j++ ) {
            tmp_col += colormap[cmap_bytes_entry * tmp_int32 + j] << (8 * j);
        }
    } else {
        tmp_col = tmp_int32;
    }
    
    return( tmp_col );
    
}





static uint32 tga_convert_color( uint32 pixel, uint32 bpp_in, ubyte alphabits, uint32 format_out ) {
    
    // this is not only responsible for converting from different depths
    // to other depths, it also switches BGR to RGB.

    // this thing will also premultiply alpha, on a pixel by pixel basis.

    ubyte r, g, b, a;

    switch( bpp_in ) {
        
    case 32:
        if( alphabits == 0 ) {
            goto is_24_bit_in_disguise;
        }
        // 32-bit to 32-bit -- nop.
        break;
        
    case 24:
is_24_bit_in_disguise:
        // 24-bit to 32-bit; (only force alpha to full)
        pixel |= 0xFF000000;
        break;

    case 15:
is_15_bit_in_disguise:
        r = (ubyte)(((float)((pixel & 0x7C00) >> 10)) * 8.2258f);
        g = (ubyte)(((float)((pixel & 0x03E0) >> 5 )) * 8.2258f);
        b = (ubyte)(((float)(pixel & 0x001F)) * 8.2258f);
        // 15-bit to 32-bit; (force alpha to full)
        pixel = 0xFF000000 + (r << 16) + (g << 8) + b;
        break;
        
    case 16:
        if( alphabits == 1 ) {
            goto is_15_bit_in_disguise;
        }
        // 16-bit to 32-bit; (force alpha to full)
        r = (ubyte)(((float)((pixel & 0xF800) >> 11)) * 8.2258f);
        g = (ubyte)(((float)((pixel & 0x07E0) >> 5 )) * 4.0476f);
        b = (ubyte)(((float)(pixel & 0x001F)) * 8.2258f);
        pixel = 0xFF000000 + (r << 16) + (g << 8) + b;
        break;
       
    }
    
    // convert the 32-bit pixel from BGR to RGB.
    pixel = (pixel & 0xFF00FF00) + ((pixel & 0xFF) << 16) + ((pixel & 0xFF0000) >> 16);

    r = pixel & 0x000000FF;
    g = (pixel & 0x0000FF00) >> 8;
    b = (pixel & 0x00FF0000) >> 16;
    a = (pixel & 0xFF000000) >> 24;
    
    // not premultiplied alpha -- multiply.
    r = (ubyte)(((float)r / 255.0f) * ((float)a / 255.0f) * 255.0f);
    g = (ubyte)(((float)g / 255.0f) * ((float)a / 255.0f) * 255.0f);
    b = (ubyte)(((float)b / 255.0f) * ((float)a / 255.0f) * 255.0f);

    pixel = r + (g << 8) + (b << 16) + (a << 24);

    /* now convert from 32-bit to whatever they want. */
    
    switch( format_out ) {
        
    case TGA_TRUECOLOR_32:
        // 32 to 32 -- nop.
        break;
        
    case TGA_TRUECOLOR_24:
        // 32 to 24 -- discard alpha.
        pixel &= 0x00FFFFFF;
        break;
        
    }

    return( pixel );

}




static int16 ttohs( int16 val ) {

#ifdef WORDS_BIGENDIAN
    return( ((val & 0xFF) << 8) + (val >> 8) );
#else
    return( val );
#endif 

}


static int16 htots( int16 val ) {

#ifdef WORDS_BIGENDIAN
    return( ((val & 0xFF) << 8) + (val >> 8) );
#else
    return( val );
#endif

}


static int32 ttohl( int32 val ) {

#ifdef WORDS_BIGENDIAN
    return( ((val & 0x000000FF) << 24) +
            ((val & 0x0000FF00) << 8)  +
            ((val & 0x00FF0000) >> 8)  +
            ((val & 0xFF000000) >> 24) );
#else
    return( val );
#endif 

}


static int32 htotl( int32 val ) {

#ifdef WORDS_BIGENDIAN
    return( ((val & 0x000000FF) << 24) +
            ((val & 0x0000FF00) << 8)  +
            ((val & 0x00FF0000) >> 8)  +
            ((val & 0xFF000000) >> 24) );
#else
    return( val );
#endif 

}
