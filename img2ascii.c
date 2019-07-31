/* TODO:
 * Use edge detection to place characters like `, ', -, /, |, etc.
 * It should add more definition to the output.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png.h"
#include "jpeglib.h"

typedef unsigned char BYTE;

typedef struct {
	BYTE **pixels;
	unsigned int width, height;
} IMG;

/* Frees the memory allocated for an image.
 * \param The image struct to be freed
*/
void img_destroy(IMG *img)
{
	for (int row = 0; row < img->height; ++row)
		free(img->pixels[row]);
	free(img->pixels);
}

/* Creates an empty pixel array of size width*height.
 * \param w Image width
 * \param h Image height
 * \return Pointer to the struct, NULL on failure.
*/
IMG *img_init(unsigned int w, unsigned int h)
{
	IMG *img = (IMG *) malloc(sizeof(IMG));
	if (img == NULL)
		return NULL;

	img->width = w;
	img->height = h;

	img->pixels = (BYTE **) malloc(sizeof(BYTE *) * w);
	if (img->pixels == NULL)
		return NULL;

	for (int col = 0; col < h; ++col) {
		img->pixels[col] = (BYTE *) calloc(h, sizeof(BYTE));
		if (img->pixels[col] == NULL) {
			img_destroy(img);
			return NULL;
		}
	}
	return img;
}

// Dumps the pixel data to stdout
void img_dump(IMG *img)
{
  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x)
      printf("%hhu ", img->pixels[x][y]);
    printf("\n");
  }
  printf("width: %u, height: %u\n", img->width, img->height);
}

/* Determines if given string ends with given ending.
 * \param str The string
 * \param end The ending
 * \return '1' if `str` ends with `end` '0' otherwise.
*/
int ends_with(const char *str, const char *end)
{
	int str_len = strlen(str);
	int end_len = strlen(end);
	if (end_len > str_len) return 0;

	int offset = str_len - end_len;
	for (int i = offset; i < str_len; ++i) {
		if (str[i] != end[i - offset])
			return 0;
	}
	return 1;
}

/* Converts a grayscale pixel value to shading character
 * TODO: Improve shading accuracy
 * \param Pixel value [0-255]
 * \return Appropriate shading character
*/
char get_shade(BYTE b)
{
  if (0 <= b && b < 50) {
    return '#';
  } else if (50 <= b && b < 100) {
    return 'm';
  } else if (100 <= b && b < 200) {
    return 'o';
  } else {
    return ' ';
  }
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	// Validate file format
	int is_jpg = ends_with(argv[1], ".jpg") | ends_with(argv[1], ".jpeg");
	int is_png = ends_with(argv[1], ".png");
	if (!(is_jpg | is_png)) {
		fprintf(stderr, "File must be either `.jp(e)g` or `.png`\n");
		return 1;
	}

	// Test input file
	FILE *infile;
	if ((infile = fopen(argv[1], "rb")) == NULL) {
		fprintf(stderr, "Cannot open `%s`\n", argv[1]);
		return 1;
	}

	// Fetch image data
	IMG *image;
	if (is_jpg) {
		// Initialize libjpeg structs
		struct jpeg_decompress_struct cinfo;
		struct jpeg_error_mgr jerr;
		cinfo.err = jpeg_std_error(&jerr);

		// Set-up & start decompression
		jpeg_create_decompress(&cinfo);
		jpeg_stdio_src(&cinfo, infile);
		jpeg_read_header(&cinfo, TRUE);
		jpeg_start_decompress(&cinfo);

		// Initialize arrays
		unsigned int img_width = cinfo.output_width;
		unsigned int img_height = cinfo.output_height;

		JSAMPARRAY scanline = (JSAMPARRAY) malloc(sizeof(JSAMPROW)); // Temp array for scanlines
		*scanline = (JSAMPROW) malloc(sizeof(JSAMPLE) * img_width * cinfo.output_components);

    image = img_init (img_width, img_height); // Final grayscale image

		// Read scanlines & convert to grayscale
    int y = 0;
		while (cinfo.output_scanline < img_height) {
			jpeg_read_scanlines(&cinfo, scanline, 1);
      for (int x = 0; x < img_width; ++x) {
        int p = x * 3;
        BYTE R = (*scanline)[p];
        BYTE G = (*scanline)[p + 1];
        BYTE B = (*scanline)[p + 2];
        image->pixels[x][y] = 0.2126 * R + 0.7152 * G + 0.0722 * B;
      }
      ++y;
		}

    // Clean-up JPEG process
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
    free(*scanline);
    free(scanline);
	} else {
    // TODO: Add support for PNG files
	}
  fclose(infile);

	// Test output file
	FILE *outfile;
	if ((outfile = fopen("out.txt", "w")) == NULL) {
		img_destroy (image);
    fprintf(stderr, "Unable to output `out.txt`\n");
		return 1;
	}

  // Convert grayscale values to characters using shademap
  for (int y = 0; y < image->height; ++y) {
    for (int x = 0; x < image->width; ++x) {
      char c = get_shade(image->pixels[x][y]);
      fprintf(outfile, "%c", c); // Write to output
    }
    fprintf(outfile, "\n");
  }

  fclose(outfile);
  img_destroy (image);
	return 0;
}
