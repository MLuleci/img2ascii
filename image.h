#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <utility>
#include <cstdio>
#include <string>
#include <array>
#include <cmath>
#include "jpeglib.h"
#include "png.h"

// Useful types
typedef unsigned char byte_t;
typedef unsigned int uint_t;

/**
 * Look for string suffix
 * @param rhs test string
 * @param lhs suffix string
 * @return true if rhs ends with lhs
 */
bool
ends_with(const std::string& rhs, const std::string& lhs)
{
    auto rs = rhs.size();
    auto ls = lhs.size();
    return !rhs.compare(rs - ls, ls, lhs);
}

/**
 * Sum of array
 * @param A
 * @return total sum
 */
template<class T, std::size_t N>
T sum(std::array<T, N> A)
{
    T x;
    for (T i : A)
        x += i;
    return x;
}

class Image
{
public:
    /** 
     * Default constructor
     * @param w width
     * @oaram h height
     * @param v value to fill image
     * @throws bad_alloc if not enough memory
     */
    Image(uint_t w = 0, uint_t h = 0, byte_t v = 0)
        : _w(w)
        , _h(h)
    {
        if (_w && _h) {
            _data = new byte_t[_w * _h] { v };
            if (!_data)
                throw std::bad_alloc();
        }
    }

    /**
     * Copy constructor
     */
    Image(const Image&) = default;

    /**
     * Move constructor
     */
    Image(Image&&) = default;

    /**
     * Copy assignment
     */
    Image& operator=(const Image&) = default;

    /**
     * Move assignment
     */
    Image& operator=(Image&&) = default;

    /**
     * Destructor
     */
    ~Image()
    {
        delete [] _data;
    }

    /** 
     * Load from file constructor
     * @param file must be of type '.jp[e]g' or '.png'
     * @throws invalid_argument if file could not be opened
     * @throws bad_alloc if not enough space for construction
     */
    Image(const std::string& file)
    {
        bool is_jpg = ends_with(file, ".jpg") 
                    | ends_with(file, ".jpeg");
        bool is_png = ends_with(file, ".png");

        if (!is_jpg && !is_png)
            throw std::invalid_argument("Unsupported file type");

        FILE *in;
        if (!(in = fopen(file.c_str(), "rb")))
            throw std::invalid_argument("Cannot open file");

        if (is_jpg)
        {
            // Initialize libjpeg structs
            struct jpeg_decompress_struct cinfo;
            struct jpeg_error_mgr jerr;
            cinfo.err = jpeg_std_error(&jerr);

            // Set-up & start decompression
            jpeg_create_decompress(&cinfo);
            jpeg_stdio_src(&cinfo, in);
            jpeg_read_header(&cinfo, TRUE);
            jpeg_start_decompress(&cinfo);

            // Get image dimensions
            _w = cinfo.output_width;
            _h = cinfo.output_height;

            // Allocate memory
            JSAMPLE* line;
            line = new JSAMPLE[_w * cinfo.output_components];
            _data = new byte_t[_w * _h];

            if (!line || !_data) {
                fclose(in);
                jpeg_abort_decompress(&cinfo);
                throw std::bad_alloc();
            }

            // Read samples, convert is necessary
            uint_t y = 0;
            while (cinfo.output_scanline < _h) {
                jpeg_read_scanlines(&cinfo, &line, 1);
                for (uint_t x = 0; x < _w; ++x) {
                    if (cinfo.out_color_space == JCS_GRAYSCALE 
                        && cinfo.output_components == 1) {
                        set(x, y, line[x]);
                    } else {
                        int p = x * 3;
                        byte_t R = line[p];
                        byte_t G = line[p + 1];
                        byte_t B = line[p + 2];
                        set(x, y, 0.2126 * R + 0.7152 * G + 0.0722 * B);
                    }
                }
                ++y;
            }

            // Clean-up JPEG process
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
            delete [] line;
        } 
        else
        {
            png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
            if (!png_ptr) {
                png_destroy_read_struct(&png_ptr, NULL, NULL);
                fclose(in);
                throw std::bad_alloc();
            } else {
                png_infop info_ptr = png_create_info_struct(png_ptr);
                if (!info_ptr) {
                    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
                    fclose(in);
                    throw std::bad_alloc();
                } else {
                    // Passed all tests
                    png_init_io(png_ptr, in);

                    // Get image data
                    png_read_info(png_ptr, info_ptr);
                    uint color_type = png_get_color_type(png_ptr, info_ptr);
                    uint bit_depth = png_get_bit_depth(png_ptr, info_ptr);
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
                    _w = png_get_image_width(png_ptr, info_ptr);
                    _h = png_get_image_height(png_ptr, info_ptr);

                    // Create image
                    _data = new byte_t[_w * _h];
                    if (!_data) {
                        fclose(in);
                        throw std::bad_alloc();
                    }

                    // FIXME: Read image
                    png_read_image(png_ptr, &_data);

                    // Clean-up PNG process
                    png_read_end(png_ptr, NULL);
                    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
                }
            }
        } 

        fclose(in);
    } // End of constructor

    /**
     * Set byte at location
     * @param x
     * @param y
     * @param v byte value
     * @return previous byte value
     * @throws out_of_range for invalid indices
     */
    byte_t set(uint_t x, uint_t y, byte_t v)
    {
        check(x, y);
        return std::exchange(_data[_w * y + x], v);
    }

    /**
     * Get byte at location
     * @param x
     * @param y
     * @return byte value
     * @throws out_of_range for invalid indices
     */
    byte_t get(uint_t x, uint_t y) const
    {
        check(x, y);
        return _data[_w * y + x];
    }

    /**
     * @return image width
     */
    uint_t width() const { return _w; }
    
    /**
     * @return image height
     */
    uint_t height() const { return _h; }

    /**
     * @return number of pixels
     */
    uint_t size() const { return _w * _h; }

    /**
     * @return beginning of data
     */
    byte_t* begin() const { return _data; }

    /**
     * @return end of data
     */
    byte_t* end() const { return _data + (_w * _h); }

private:
    /**
     * Helper for checking indices
     * @param x
     * @param y
     * @throws out_of_range for invalid indices
     */
    void check(uint_t x, uint_t y) const
    {
        if (x >= _w || y >= _h)
            throw std::out_of_range("Indices out of range");
    }

    byte_t* _data;
    uint_t _w, _h;
};

/**
 * Calcualte image histogram
 * @param im input image
 * @return histogram array
 */
std::array<byte_t, 255> hist(const Image& im)
{
    std::array<byte_t, 255> H { 0 };
    for (byte_t b : im)
        H[b]++;
    return H;
}

/**
 * Binarize image using Otsu's method
 * The returned image's members are still of type
 * byte_t, set to zero and non-zero accordingly.
 *
 * https://en.wikipedia.org/wiki/Otsu%27s_method
 *
 * @param im input image
 * @return binary image
 */
Image otsu(const Image& im)
{
    // Histogram et al.
    auto H = hist(im);
    double dot; // dot product of 0:255 and H

    for (size_t i = 0; i < H.size(); ++i)
        dot += i * H[i];

    // Calculate threshold:
    // wi -> class weights
    // mi -> class means
    // th -> threshold
    // sz -> image size
    // ind -> gray level
    double w0, w1, m0, m1, th, sz;
    size_t ind;

    w0 = 0;
    m0 = 0;
    th = 0;
    sz = (double) im.size();
    for (size_t i = 0; i < H.size(); ++i)
    {
        w1 = sz - w0;
        if (w0 > 0 && w1 > 0) {
            // Calculate inter-class variance
            m1 = (dot - m0) / w1;
            double v = w0 * w1 * pow((m0 / w0) - m1, 2.0);

            // Maximize variance
            if (v >= th) {
                ind = i;
                th = v;
            }
        }
        
        // Update w0 & m0 incrementally
        w0 = w0 + H[i];
        m0 = m0 + i * H[i];
    }

    // Binarize
    Image om(im.width(), im.height());
    std::transform(im.begin(), im.end(), om.begin(),
            [&] (byte_t b) -> byte_t 
            { return (byte_t) (b > ind); });
    return om;
}

/**
 * Convert image into ascii art
 * Uses unicode braille characters.
 *
 * @param im input image
 * @return ascii art
 */
std::string ascii(const Image& im)
{
    auto om = otsu(im);
    std::string s;
    size_t w, h;

    // Pre-allocate string
    w = ceil(om.width() / 2) + 1;
    h = ceil(om.height() / 3);
    s.reserve(w * h);

    // Lump together groups of 2x3 pixels
    for (size_t y = 0; y < om.height() - 2; y += 3)
    {
        for (size_t x = 0; x < om.width() - 2; x += 2)
        {
            byte_t b = om.get(x, y)
                     | om.get(x, y + 1) << 1
                     | om.get(x, y + 2) << 2
                     | om.get(x + 1, y) << 3
                     | om.get(x + 1, y + 1) << 4
                     | om.get(x + 1, y + 2) << 5;

            s.append({
                (char) 0xE2,
                (char) 0xA0,
                (char) (0x80 + b)
            });
        }
        s += '\n';
    }

    return s;
}
