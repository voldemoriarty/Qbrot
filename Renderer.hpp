#include "avxVector.hpp"

#include "EasyBMP.hpp"

struct ColorScheme {
  EasyBMP::RGBColor mapping [16];

  ColorScheme () {
    mapping[0].SetColor(66, 30, 15);
    mapping[1].SetColor(25, 7, 26);
    mapping[2].SetColor(9, 1, 47);
    mapping[3].SetColor(4, 4, 73);
    mapping[4].SetColor(0, 7, 100);
    mapping[5].SetColor(12, 44, 138);
    mapping[6].SetColor(24, 82, 177);
    mapping[7].SetColor(57, 125, 209);
    mapping[8].SetColor(134, 181, 229);
    mapping[9].SetColor(211, 236, 248);
    mapping[10].SetColor(241, 233, 191);
    mapping[11].SetColor(248, 201, 95);
    mapping[12].SetColor(255, 170, 0);
    mapping[13].SetColor(204, 128, 0);
    mapping[14].SetColor(153, 87, 0);
    mapping[15].SetColor(106, 52, 3);
  }

  EasyBMP::RGBColor Color (int itr) {
    return mapping[itr % 16];
  }
};

struct Renderer {
private:
  float xl, xh, yl, yh;
  float xres, yres;
  int   maxIters, width, height;

public:
  void Configure(float xl, float xh, float yl, float yh, int maxIters, int width, int height) {
    this->xl = xl;
    this->xh = xh;
    this->yl = yl;
    this->yh = yh;
    this->xres = (xh - xl) / width;
    this->yres = (yh - yl) / height;
    this->width  = width;
    this->height = height;
    this->maxIters = maxIters;
  } 

  // render a horizontal line where 0 is the top
  // the result pointer must be aligned to 8 word boundary (32 byte)
  void RenderLine(int i, ColorScheme &scheme, EasyBMP::Image &img) {
    auto c1 = cvec8f(xl, xres, (i) * yres - yh);
    auto c2 = cvec8f(cvec8f::len() * xres + xl, xres, (i) * yres - yh);
    
    for (int xc = 0; xc < width; xc += 2 * cvec8f::len()) {
      auto z1 = cvec8f(0);
      auto z2 = cvec8f(0);

      auto count1 = Counter8i();
      auto count2 = Counter8i();

      bool run1 = true;
      bool run2 = true;

      while (run1 | run2) {
        z1 = z1.square() + c1;
        z2 = z2.square() + c2;
        run1 = count1.increment(maxIters, z1.magLessThan(4));
        run2 = count2.increment(maxIters, z2.magLessThan(4));
      }

      c1 = c1 + cvec8f(2 * cvec8f::len() * xres); 
      c2 = c2 + cvec8f(2 * cvec8f::len() * xres); 
      
      int res[16] __attribute__ ((aligned(32)));
      store(count1.count, res);
      store(count2.count, res + cvec8f::len());

      for (int c = 0; c < 2 * cvec8f::len(); ++c) {
        img.SetPixel(xc + c, i, scheme.Color(res[c]));
      }
    }
  }

  void RenderImage(EasyBMP::Image &img) {
    ColorScheme scheme;
    
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; ++y) {
      RenderLine(y, scheme, img);
    }
  }
};