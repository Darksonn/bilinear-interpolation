#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <zlib.h>
#include <png.h>

// x1, y1, x2, y2 forms a rectangle of coordinates in the image
// v11 represents the value at x1, y1
// v12 represents the value at x1, y2
// v21 represents the value at x2, y1
// v22 represents the value at x2, y2
struct bilinear_interpolation {
  double x1, y1, x2, y2;
  double v11, v12, v21, v22;
};

// approximates the value of the function f at x,y using bilinear interpolation
double interpolate(double x, double y, struct bilinear_interpolation *f) {
  double dx1 = x - f->x1;
  double dx2 = f->x2 - x;
  double dy1 = y - f->y1;
  double dy2 = f->y2 - y;
  double scaled = f->v11 * dx2 * dy2
                + f->v21 * dx1 * dy2
                + f->v12 * dx2 * dy1
                + f->v22 * dx1 * dy1;
  return scaled / ((f->x2 - f->x1) * (f->y2 - f->y1));
}

struct rgba {
  double r, g, b, a;
};
struct linearized_rgba {
  double r, g, b, a;
};
struct bilinear_interpolation_rgba {
  double x1, y1, x2, y2;
  struct linearized_rgba v11, v12, v21, v22;
};

// taking the average of two rgba colors is not as simple as it sounds
// in order to correctly take the average, one must linearize the color first
struct linearized_rgba linearize(struct rgba rgba) {
  struct linearized_rgba lrgba;
  lrgba.r = rgba.r * rgba.r;
  lrgba.g = rgba.g * rgba.g;
  lrgba.b = rgba.b * rgba.b;
  lrgba.a = rgba.a; // alpha is already linear
  return lrgba;
}
// taking the average of two rgba colors is not as simple as it sounds
// in order to correctly take the average, one must linearize the color first
struct rgba delinearize(struct linearized_rgba lrgba) {
  struct rgba rgba;
  rgba.r = sqrt(lrgba.r);
  rgba.g = sqrt(lrgba.g);
  rgba.b = sqrt(lrgba.b);
  rgba.a = lrgba.a; // alpha is already linear
  return rgba;
}

struct linearized_rgba linearized_rgba_interpolate(double x, double y, struct bilinear_interpolation_rgba *f) {
  struct bilinear_interpolation ff;
  ff.x1 = f->x1;
  ff.x2 = f->x2;
  ff.y1 = f->y1;
  ff.y2 = f->y2;

  struct linearized_rgba result;

  ff.v11 = f->v11.r;
  ff.v12 = f->v12.r;
  ff.v21 = f->v21.r;
  ff.v22 = f->v22.r;
  result.r = interpolate(x, y, &ff);

  ff.v11 = f->v11.g;
  ff.v12 = f->v12.g;
  ff.v21 = f->v21.g;
  ff.v22 = f->v22.g;
  result.g = interpolate(x, y, &ff);

  ff.v11 = f->v11.b;
  ff.v12 = f->v12.b;
  ff.v21 = f->v21.b;
  ff.v22 = f->v22.b;
  result.b = interpolate(x, y, &ff);

  ff.v11 = f->v11.a;
  ff.v12 = f->v12.a;
  ff.v21 = f->v21.a;
  ff.v22 = f->v22.a;
  result.a = interpolate(x, y, &ff);

  return result;
}

struct rgba rgba_interpolate(double x, double y, struct bilinear_interpolation_rgba *f) {
  struct linearized_rgba res = linearized_rgba_interpolate(x, y, f);
  return delinearize(res);
}


// the code below is simply for testing

//http://www.labbookpages.co.uk/software/imgProc/libPNG.html
int writeImage(char* filename, int width, int height, char* title, struct bilinear_interpolation_rgba *f) {
  int code = 0;
  FILE *fp = NULL;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep row = NULL;
  // Open file for writing (binary mode)
  fp = fopen(filename, "wb");
  if (fp == NULL) {
    fprintf(stderr, "Could not open file %s for writing\n", filename);
    code = 1;
    goto finalise;
  }
  // Initialize write structure
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL) {
    fprintf(stderr, "Could not allocate write struct\n");
    code = 1;
    goto finalise;
  }

  // Initialize info structure
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    fprintf(stderr, "Could not allocate info struct\n");
    code = 1;
    goto finalise;
  }
  // Setup Exception handling
  if (setjmp(png_jmpbuf(png_ptr))) {
    fprintf(stderr, "Error during png creation\n");
    code = 1;
    goto finalise;
  }
  png_init_io(png_ptr, fp);

  // Write header (8 bit colour depth)
  png_set_IHDR(png_ptr, info_ptr, width, height,
      8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  // Set title
  if (title != NULL) {
    png_text title_text;
    title_text.compression = PNG_TEXT_COMPRESSION_NONE;
    title_text.key = "Title";
    title_text.text = title;
    png_set_text(png_ptr, info_ptr, &title_text, 1);
  }

  png_write_info(png_ptr, info_ptr);   // Allocate memory for one row (3 bytes per pixel - RGB)
  row = (png_bytep) malloc(3 * width * sizeof(png_byte));

  // Write image data
  int x, y;
  for (y=0 ; y<height ; y++) {
    for (x=0 ; x<width ; x++) {

      struct rgba color = rgba_interpolate(x, y, f);
      row[3*x+0] = (char) (color.r * 255.0);
      row[3*x+1] = (char) (color.g * 255.0);
      row[3*x+2] = (char) (color.b * 255.0);

    }
    png_write_row(png_ptr, row);
  }

  // End write
  png_write_end(png_ptr, NULL);
finalise:
  if (fp != NULL) fclose(fp);
  if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
  if (row != NULL) free(row);

  return code;
}

int main(void) {
  struct bilinear_interpolation_rgba f;

  f.x1 = 0;
  f.y1 = 0;
  f.x2 = 256;
  f.y2 = 256;

  struct rgba color;
  color.a = 1.0;
  color.r = 1.0; color.g = 0.0; color.b = 0.0;
  f.v11 = linearize(color);
  color.r = 0.0; color.g = 0.0; color.b = 1.0;
  f.v12 = linearize(color);
  color.r = 1.0; color.g = 0.0; color.b = 0.0;
  f.v21 = linearize(color);
  color.r = 0.0; color.g = 1.0; color.b = 0.0;
  f.v22 = linearize(color);

  return writeImage("image.png", 256, 256, "interpolate", &f);
}
