#include "Renderer.hpp"

#include <chrono>
#include <thread>

auto RenderWithTime (EasyBMP::Image &img, Renderer &r, int runs = 1) {
    // find the number of available threads
    const int pars = std::thread::hardware_concurrency();
    std::cout << "Using " << pars << " threads\n";

    auto start = std::chrono::high_resolution_clock::now();
    while (runs--) r.RenderImage(img);
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    return dur;
}

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
  Renderer r;
  r.Configure(xl, xh, yl, yh, M, w, h);

  auto dur = RenderWithTime(bmp, r, C);

  std::cout << 
    "Rendering done\n"
    "Avg time per frame: " 
    << (dur / C) * 1e-6 << " (ms)\n"
    "Total time taken:   "
    << dur * 1e-6 << " (ms)\n";
  
  std::flush(std::cout);

  bmp.Write();
  return 0;
}