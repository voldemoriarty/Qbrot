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

  void SectionRenderer (int begin, int end, EasyBMP::Image *buff) {
    for (int i = begin; i < end; i++) {
      using fvec_t = __m256;
      using ivec_t = __m256i;

      fvec_t x0 = _mm256_add_ps(_mm256_set1_ps(xl), _mm256_setr_ps(0, xres, 2*xres, 3*xres, 4*xres, 5*xres, 6*xres, 7*xres));
      fvec_t y0 = _mm256_set1_ps((i) * yres - yh);

      for (int xc = 0; xc < width; xc += 8) {
        ivec_t itr  = _mm256_setzero_si256();
        fvec_t x    = _mm256_setzero_ps();
        fvec_t y    = _mm256_setzero_ps();
        fvec_t ab   = _mm256_setzero_ps();

        fvec_t aCmp = _mm256_setzero_ps();
        ivec_t iCmp = _mm256_setzero_si256();
        int aMask = 0xff, iMask = 0;

        while ((aMask & (~iMask & 0xff)) != 0) {
          auto xx   = _mm256_mul_ps(x, x);
          auto yy   = _mm256_mul_ps(y, y);
          auto xyn  = _mm256_mul_ps(x, y);
          auto xy   = _mm256_mul_ps(xyn, _mm256_set1_ps(2.0f));
          auto xn   = _mm256_sub_ps(xx, yy);
          ab        = _mm256_add_ps(xx, yy);
          y         = _mm256_add_ps(xy, y0);
          x         = _mm256_add_ps(xn, x0);
          aCmp      = _mm256_cmp_ps(ab, _mm256_set1_ps(4), _CMP_LT_OQ);
          iCmp      = _mm256_cmpeq_epi32(itr, _mm256_set1_epi32(max));
          aMask     = _mm256_movemask_ps(aCmp) & 0xff;
          iMask     = _mm256_movemask_ps((fvec_t)iCmp) & 0xff;

          // only add one to the iterations of those whose ab < 4 and itr < max
          // aCmp = 1 for ab < 4
          // iCmp = 0 for itr < max
          auto inc  = _mm256_andnot_ps((fvec_t)iCmp, aCmp);
          // inc = -1 for (itr < max) & (ab < 4)
          // itr = itr - inc [- (-1) = + 1]
          itr       = _mm256_sub_epi32(itr, (ivec_t)inc);
        }

        x0 = _mm256_add_ps(x0, _mm256_set1_ps(8*xres));
        // collect results and update buffer
        int res[8] __attribute__ ((aligned));
        _mm256_store_si256((ivec_t*)res, itr);
        for (int c = 0; c < 8; ++c) {
          buff->SetPixel(xc + c, i, cs.Color(res[c]));
        }
      }
    }
  }

  void RenderMultiThreaded (EasyBMP::Image *buff, int pars) {
    // divide the image in number of pars sections and assign each thread
    // a section. Wait for them to complete

    std::vector <std::thread> threads(pars);
    const int linesPerThread = height / pars;

    for (int i = 0; i < pars; ++i) {
      threads[i] = std::thread([=]() {
        SectionRenderer(i*linesPerThread, (i+1)*linesPerThread, buff);
      });
    }

    // now the threads are launched
    // wait for them to finish
    for (auto &thread : threads) {
      thread.join();
    }
  }

  auto RenderWithTime (EasyBMP::Image *buff, int runs = 1) {
    // find the number of available threads
    const int pars = std::thread::hardware_concurrency();
    std::cout << "Using " << pars << " threads\n";

    auto start = std::chrono::high_resolution_clock::now();
    while (runs--) RenderMultiThreaded(buff, pars);
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
    "Avg time: " 
    << (dur / C) * 1e-6 << " (ms)\n"
    "Tot time: "
    << dur * 1e-6 << " (ms)\n";

  bmp.Write();
  return 0;
}

