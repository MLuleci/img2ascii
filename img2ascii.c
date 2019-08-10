#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <math.h>
#include "png.h"
#include "jpeglib.h"

#define PI 3.14159265

typedef unsigned char BYTE;

typedef struct {
	BYTE **pixels;
	unsigned int width, height;
} IMG;

/* Frees the memory allocated for an image.
 * \param img The image struct to be freed
*/
void img_destroy(IMG *img)
{
	for (int row = 0; row < img->height; ++row)
		free(img->pixels[row]);
	free(img->pixels);
	free(img);
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

	img->pixels = (BYTE **) malloc(sizeof(BYTE *) * h);
	if (img->pixels == NULL) {
		free(img);
		return NULL;
	}

	for (int row = 0; row < h; ++row) {
		img->pixels[row] = (BYTE *) calloc(w, sizeof(BYTE));
		if (img->pixels[row] == NULL) {
			img_destroy(img);
			return NULL;
		}
	}
	return img;
}

/* Dumps the pixel data to stdout.
 * \param img The image being dumped
*/
void img_dump(IMG *img)
{
	for (int y = 0; y < img->height; ++y) {
		for (int x = 0; x < img->width; ++x)
			printf("%hhu ", img->pixels[y][x]);
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

/* Converts a grayscale pixel value to shading character.
 * \param b The pixel value in range [0-255]
 * \return Appropriate shading character
*/
wchar_t get_shade(BYTE b)
{
	if (0 <= b && b < 51) {
		return L'█';
	} else if (51 <= b && b < 102) {
		return L'▓';
	} else if (102 <= b && b < 153) {
		return L'▒';
	} else if (153 <= b && b < 204) {
		return L'░';
	} else {
		return L' ';
	}
}

/* Helper function that performs convolsion between the kernels and pixels.
 * \param kernel 2D array of weights
 * \param img The pixel data
 * \param x Horizontal coordinate on the image
 * \param y Vertical coordinate on the image
 * \return The resulting sum of the convolsion
*/
int convolve(int kernel[3][3], IMG *img, int x, int y)
{
	int sum = 0;
	for (int ky = -1; ky < 2; ++ky) {
		for (int kx = -1; kx < 2; ++kx) {
			int px = (x + kx < 0 || x + kx >= img->width ? x : x + kx);
			int py = (y + ky < 0 || y + ky >= img->height ? y : y + ky);
			sum += img->pixels[py][px] * kernel[ky + 1][kx + 1];
		}
	}
	return sum;
}

/* Applies a Sobel filter on the given image and returns the edge map.
 * Each pixel on the map either has a value of 0 if the pixel isn't part of
 * an edge or a value [1-255] that is a percentage to be multiplied with 2*PI
 * to obtain the gradient at that pixel. Note that the range is actually between
 * [0-254] as 0 is reserved for non-edge pixels.
 * e.g. A value of 127 means: [(127 - 1)/254] * 2 * PI = 3.129 rad (or 179.3 deg)
 * \param img The supplied image
 * \return The edge map produced
*/
IMG *sobel(IMG *img)
{
	int Gx[3][3] =
	{
		{-1, 0, 1},
		{-2, 0, 2},
		{-1, 0, 1}
	};
	int Gy[3][3] =
	{
		{-1, -2, -1},
		{0, 0, 0},
		{1, 2, 1}
	};
	double thresh = 120.0; // TODO: Determine thresh using image histogram instead.

	IMG *map = img_init(img->width, img->height);	for (int y = 0; y < img->height; ++y) {
		for (int x = 0; x < img->width; ++x) {
			int sx = convolve(Gx, img, x, y);
			int sy = convolve(Gy, img, x, y);
			double mag = sqrt(pow(sx, 2) + pow(sy, 2));
			if (mag > thresh) {
				double dir = atan2(sy, sx);
				if (dir < 0) dir += 2 * PI;
				map->pixels[y][x] = (BYTE) round((dir * 254) / (2 * PI));
			}
		}
	}

	return map;
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "C.UTF-8");

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

		// Get image data
		unsigned int img_width = cinfo.output_width;
		unsigned int img_height = cinfo.output_height;

		// Create arrays
		JSAMPARRAY scanline = (JSAMPARRAY) malloc(sizeof(JSAMPROW)); // Temp array for scanlines
		*scanline = (JSAMPROW) malloc(sizeof(JSAMPLE) * img_width * cinfo.output_components);
		image = img_init(img_width, img_height); // Final grayscale image

		// Read scanlines & convert to grayscale
		int y = 0;
		while (cinfo.output_scanline < img_height) {
			jpeg_read_scanlines(&cinfo, scanline, 1);
			for (int x = 0; x < img_width; ++x) {
				if (cinfo.out_color_space == JCS_GRAYSCALE && cinfo.output_components == 1) {
					image->pixels[y][x] = (*scanline)[x];
				} else {
					int p = x * 3;
					BYTE R = (*scanline)[p];
					BYTE G = (*scanline)[p + 1];
					BYTE B = (*scanline)[p + 2];
					image->pixels[y][x] = 0.2126 * R + 0.7152 * G + 0.0722 * B;
				}
			}
			++y;
		}

		// Clean-up JPEG process
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		free(*scanline);
		free(scanline);
	} else {
		png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (!png_ptr) {
			png_destroy_read_struct(&png_ptr, NULL, NULL);
			fclose(infile);
			fprintf(stderr, "Internal error (png_destroy_read_struct)\n");
			return 1;
		} else {
			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr) {
				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
				fclose(infile);
				fprintf(stderr, "Internal error (png_create_info_struct)\n");
				return 1;
			} else {
				// Passed all tests
				png_init_io(png_ptr, infile);

				// Get image data
				png_read_info(png_ptr, info_ptr);
				unsigned int color_type = png_get_color_type(png_ptr, info_ptr);
				unsigned int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
				png_set_strip_16(png_ptr);

				// Do transformations
				int need_bg = 0;
				int need_conv = 0;
				switch(color_type) {
					case PNG_COLOR_TYPE_GRAY:
					{
						if (bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
						if (bit_depth == 16) png_set_strip_16(png_ptr);
						break;
					}

					case PNG_COLOR_TYPE_GRAY_ALPHA:
					{
						if (bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
						if (bit_depth == 16) png_set_strip_16(png_ptr);
						if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
							png_set_tRNS_to_alpha(png_ptr);
						need_bg = 1;
						break;
					}

					case PNG_COLOR_TYPE_RGB:
						need_conv = 1;
						break;

					case PNG_COLOR_TYPE_RGBA:
					{
						need_conv = 1;
						need_bg = 1;
						break;
					}

					case PNG_COLOR_TYPE_PALETTE:
					{
						png_set_palette_to_rgb(png_ptr);
						if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
							png_set_tRNS_to_alpha(png_ptr);
							need_bg = 1;
						}
						need_conv = 1;
						break;
					}
				}

				// Overlay image on white background
				// FIXME: Transparent pixels not converted to white
				png_color_16 bg;
				png_color_16p img_bg;
				bg.red = 0xFF;
				bg.green = 0xFF;
				bg.blue = 0xFF;
				if (png_get_bKGD(png_ptr, info_ptr, &img_bg)) {
    				png_set_background(png_ptr, img_bg, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
				} else if (need_bg) {
					png_set_background(png_ptr, &bg, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
				}

				// Convert to grayscale
				if (need_conv) png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);

				// Get image data (post-transforms)
				png_read_update_info(png_ptr, info_ptr);
				unsigned int img_width = png_get_image_width(png_ptr, info_ptr);
				unsigned int img_height = png_get_image_height(png_ptr, info_ptr);

				// Create image
				image = img_init(img_width, img_height);

				// Read image
				png_read_image(png_ptr, image->pixels);

				// Clean-up PNG process
				png_read_end(png_ptr, NULL);
				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			}
		}
	}
	fclose(infile);

	// Test output file
	FILE *outfile;
	if ((outfile = fopen("out.txt", "w")) == NULL) {
		img_destroy(image);
		fprintf(stderr, "Unable to output `out.txt`\n");
		return 1;
	}

	// Create edge map
	IMG *edgemap = sobel(image);

	// Convert grayscale values to characters
	for (int y = 0; y < image->height; ++y) {
		for (int x = 0; x < image->width; ++x) {
			if (edgemap->pixels[y][x] > 0) {
				double angle = ((double) (edgemap->pixels[y][x] - 1) / 254.0) * 2.0 * PI;
				wchar_t c = L"-/|\\"[(int) round(angle / (PI / 4)) % 4];
				fputwc(c, outfile);
			} else {
				wchar_t c = get_shade(image->pixels[y][x]);
				fputwc(c, outfile); // Write to output
			}
		}
		fputwc(L'\n', outfile);
	}

	fclose(outfile);
	img_destroy(image);
	img_destroy(edgemap);
	return 0;
}
