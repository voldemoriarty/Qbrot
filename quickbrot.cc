#include <immintrin.h>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

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

template <class CS = ColorScheme>
struct MandelbrotMultiThreaded {
  int height;
  int width;
  int max;

  float xres;
  float yres;
  float x0, y0;
  float xl, yl, yh;

  CS cs;

  void Configure (
    int   height, 
    int   width,
    int   max = 128,
    float xl = -2.5,
    float xh = 1,
    float yl = -1,
    float yh = 1 
  ) {
    this->x0      = xl;
    this->y0      = yl;
    this->xl      = xl;
    this->yl      = yl;
    this->yh      = yh;
    this->xres    = (xh - xl) / width;
    this->yres    = (yh - yl) / height;
    this->max     = max;
    this->width   = width;
    this->height  = height;
  }

  void LineRenderer (int i, EasyBMP::Image *buff) {
    using fvec_t = __m256;
    using ivec_t = __m256i;

    fvec_t x01 = _mm256_add_ps(_mm256_set1_ps(xl), _mm256_setr_ps(0, xres, 2*xres, 3*xres, 4*xres, 5*xres, 6*xres, 7*xres));
    fvec_t x02 = _mm256_add_ps(_mm256_set1_ps(8*xres + xl), _mm256_setr_ps(0, xres, 2*xres, 3*xres, 4*xres, 5*xres, 6*xres, 7*xres));
    fvec_t y0 = _mm256_set1_ps((i) * yres - yh);

    for (int xc = 0; xc < width; xc += 16) {
      ivec_t itr1  = _mm256_setzero_si256();
      ivec_t itr2  = _mm256_setzero_si256();
      fvec_t x1    = _mm256_setzero_ps();
      fvec_t x2    = _mm256_setzero_ps();
      fvec_t y1    = _mm256_setzero_ps();
      fvec_t y2    = _mm256_setzero_ps();
      fvec_t ab1   = _mm256_setzero_ps();
      fvec_t ab2   = _mm256_setzero_ps();

      ivec_t aCmp1 = _mm256_setzero_si256();
      ivec_t aCmp2 = _mm256_setzero_si256();
      ivec_t iCmp1 = _mm256_setzero_si256();
      ivec_t iCmp2 = _mm256_setzero_si256();
      
      int cond1 = 1;
      int cond2 = 1;

      // manually unrolled loop by factor of 2
      // decreases register dependency hazards and increases perf
      while (cond1 | cond2) {
        auto xx1   = _mm256_mul_ps(x1, x1);
        auto xx2   = _mm256_mul_ps(x2, x2);
        auto yy1   = _mm256_mul_ps(y1, y1);
        auto yy2   = _mm256_mul_ps(y2, y2);
        auto xyn1  = _mm256_mul_ps(x1, y1);
        auto xyn2  = _mm256_mul_ps(x2, y2);
        auto xy1   = _mm256_add_ps(xyn1, xyn1);
        auto xy2   = _mm256_add_ps(xyn2, xyn2);
        auto xn1   = _mm256_sub_ps(xx1, yy1);
        auto xn2   = _mm256_sub_ps(xx2, yy2);
        ab1        = _mm256_add_ps(xx1, yy1);
        ab2        = _mm256_add_ps(xx2, yy2);
        y1         = _mm256_add_ps(xy1, y0);
        y2         = _mm256_add_ps(xy2, y0);
        x1         = _mm256_add_ps(xn1, x01);
        x2         = _mm256_add_ps(xn2, x02);
        aCmp1      = _mm256_castps_si256(_mm256_cmp_ps(ab1, _mm256_set1_ps(4), _CMP_LT_OQ));
        aCmp2      = _mm256_castps_si256(_mm256_cmp_ps(ab2, _mm256_set1_ps(4), _CMP_LT_OQ));
        iCmp1      = _mm256_cmpeq_epi32(itr1, _mm256_set1_epi32(max));
        iCmp2      = _mm256_cmpeq_epi32(itr2, _mm256_set1_epi32(max));
        cond1      = _mm256_testc_si256(iCmp1, aCmp1) == 0; 
        cond2      = _mm256_testc_si256(iCmp2, aCmp2) == 0;
        // only add one to the iterations of those whose ab < 4 and itr < max
        // aCmp = 1 for ab < 4
        // iCmp = 0 for itr < max
        auto inc1  = _mm256_andnot_si256(iCmp1, aCmp1);
        auto inc2  = _mm256_andnot_si256(iCmp2, aCmp2);
        // inc = -1 for (itr < max) & (ab < 4)
        // itr = itr - inc [- (-1) = + 1]
        itr1       = _mm256_sub_epi32(itr1, inc1);
        itr2       = _mm256_sub_epi32(itr2, inc2);
      }

      x01 = _mm256_add_ps(x01, _mm256_set1_ps(16*xres));
      x02 = _mm256_add_ps(x02, _mm256_set1_ps(16*xres));
      // collect results and update buffer
      int res[16] __attribute__ ((aligned(32)));
      _mm256_store_si256((ivec_t*)res, itr1);
      _mm256_store_si256((ivec_t*)(res + 8), itr2);
      for (int c = 0; c < 16; ++c) {
        buff->SetPixel(xc + c, i, cs.Color(res[c]));
      }
    }
  }

  void RenderMultiThreaded (EasyBMP::Image *buff) {
    // use openMP to handle threading for us
    // tell the compiler to create a task queue
    // it is faster because different sections of the set take different time
    // so dynamic scheduling performs better than static
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; ++y) {
      LineRenderer(y, buff);
    }
  }

  auto RenderWithTime (EasyBMP::Image *buff, int runs = 1) {
    // find the number of available threads
    const int pars = std::thread::hardware_concurrency();
    std::cout << "Using " << pars << " threads" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    while (runs--) RenderMultiThreaded(buff);
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    return dur;
  }
};

int main (int argc, char **argv) {

  int w = 640;
  int h = 480;
  int M = 128;
  int C = 1;

  float xl = -2.5;
  float xh = 1;
  float yl = -1;
  float yh = 1;

  if (argc >= 3) {
    w = atoi(argv[1]);
    h = atoi(argv[2]);
  }

  if (argc >= 4) {
    M = atoi(argv[3]);
  }

  if (argc >= 5) {
    C = atoi(argv[4]);
  }

  EasyBMP::Image bmp (w, h, "render.bmp");
  MandelbrotMultiThreaded<ColorScheme> mb;
  mb.Configure(h,w,M,xl,xh,yl,yh);

  auto dur = mb.RenderWithTime(&bmp, C);

  std::cout << 
    "Rendering done\n"
    "Avg time per frame: " 
    << (dur / C) * 1e-6 << " (ms)\n"
    "Total time taken:   "
    << dur * 1e-6 << " (ms)" << std::endl;

  bmp.Write();
  return 0;
}

