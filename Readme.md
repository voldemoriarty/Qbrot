# Quickbrot: Quickly rendering the mandelbrot set

Quickbrot is a multithreaded and vectorized renderer for the mandelbrot set written in C++. Uses AVX2 intrinsics to calculate 8 pixels in parallel per thread. Automatically divides the image into multiple threads. The result is a renderer that is very fast (A 1080p render of the region x:[-2.5,1], y:[-1,1] with 256 maximum iterations takes only ~19 ms on a 2 core 4 thread core i5-6200u laptop running Kubuntu 19.04). Uses the EasyBMP header library to create BMP image.
## Compile and Use
```bash
# Compile with 
g++ -O3 -fopenmp -mavx2 quickbrot.cc -o qbrot

# to check if your CPU supports AVX2
lscpu | grep avx2

# Usage:
# render a 640x480 with 64 iterations
./qbrot  

# render w x h with maxIter iterations N times and compute average frame time 
./qbrot $w $h $maxIter $N
./qbrot 1920 1080 256 10
```

## How it works
The core of the program is the mandelbrot loop. It computes 8 pixels in parallel by counting the number of iterations required to determine if a point lies in the set or not. A simple implementation in scala would be 

```scala
  def pixelValue (x_pixel: Int, y_pixel: Int): Int = {
    val x0 = x_pixel * xres + xrange._1
    val y0 = y_pixel * yres - yrange._2
    var i = 0
    var x,y = 0.0f
    var xx,yy,xy = 0.0f
    var ab = 0.0f

    while (ab < 4.0f && i < maxItr) {
      xx = x * x
      yy = y * y
      xy = 2.0f * x * y
      i  = i + 1
      ab = xx + yy
      x  = xx - yy
      y  = xy + y0
      x  = x + x0
    }
    i
  }
  ```
  ### Multithreading
  This computation is embarrassingly parallel. All pixels can be computed independant of each other. The first step in parallelizing this would be to use multiple threads and compute one pixel per thread. A simple implementation in scala would be to use parallel collections.

  ```scala 
    def render (): Unit = {
      for (x <- 0 until width)
        (0 until height).par.foreach(y => buffer.setRGB(x, y, rgbToInt(core.pixelColor(x, y))))
    }
  ```
  ### Vectorization
  Modern x86 CPUs contain ALUs that can work on multiple data units in a single instruction. They are called SIMD (single instruction multiple data) instructions. AVX2 is an extension with 256 bit ALUs. Meaning they can work with 8 32 bit `float`s  in parallel. We can use them to speed up the rendering. 
  
  Unfortunately scala does not provide a mechanism for using AVX instructions so I chose to do it in C++. C++ provides compiler intrinsics (which are functions that map to a single assembly instruction) for AVX in the `<immintrin.h>` header file.

  The vectorized code for the loop in C++ is 
  ```C++
    // aMask's lower 8 bits correspond to the comparison ab < 4 for 8 pixels
    // iMast's lower 8 bits correspond to itr == MAX so it has to be inverted
    // the loop is run till all the 8 pixels have been computed
    while ((aMask & (~iMask & 0xff)) != 0) {
      auto xx   = _mm256_mul_ps(x, x);
      auto yy   = _mm256_mul_ps(y, y);
      auto xyn  = _mm256_mul_ps(x, y);
      auto xy   = _mm256_add_ps(xyn, xyn);
      auto xn   = _mm256_sub_ps(xx, yy);
      ab        = _mm256_add_ps(xx, yy);
      y         = _mm256_add_ps(xy, y0);
      x         = _mm256_add_ps(xn, x0);
      aCmp      = _mm256_cmp_ps(ab, _mm256_set1_ps(4), _CMP_LT_OQ); // 0xffffffff for true 0 for false
      iCmp      = _mm256_cmpeq_epi32(itr, _mm256_set1_epi32(max));
      aMask     = _mm256_movemask_ps(aCmp) & 0xff;  // set corresponding bit in aMask if MSB is set
      iMask     = _mm256_movemask_ps((fvec_t)iCmp) & 0xff;

      // only add one to the iterations of those whose ab < 4 and itr < max
      // aCmp = 1 for ab < 4
      // iCmp = 0 for itr < max
      auto inc  = _mm256_andnot_ps((fvec_t)iCmp, aCmp);
      // inc = -1 for (itr < max) & (ab < 4)
      // itr = itr - inc [- (-1) = + 1]
      itr       = _mm256_sub_epi32(itr, (ivec_t)inc);
    }
  ```

  The above loop has tight register dependencies (too many results dependant on previous results) so the cpu has to wait for results to be computed before using them in the next instructions. To solve this we can unroll the loop manually so that in each iteration, 16 pixels are computed instead of 8. The unrolled loop is
  ```C++
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
  ```

  ### C++ Multithreading
  To completely parallelize the render, the program divides the image into sections and assigns a section to a single thread. A section contains >= 1 horizontal lines to render. A single line is rendered 8 pixels at a time. So on a 4 thread CPU, 32 pixels are being rendered at a time. 
  ```C++
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
  ```

  ## Render Times
  The timings shown below were measured on a 2 core 4 thread core i5-6200U laptop CPU running Kubuntu 19.04 with 8GB RAM. Scala implementation is multithreaded whereas C++ implementation is vectorized and multithreaded.
  
  
|  Resolution 	| Max Iterations 	| Scala Frame Time  (ms) 	| C++ Frame Time (ms) 	|
|---------------|-----------------|-------------------------|-----------------------|
| 1920 x 1080 	|       64       	|         281.99         	|        7.83         	|
| 1920 x 1080 	|       256      	|         441.32         	|        19.6         	|
| 1920 x 1080 	|      1024      	|         1073.49        	|        68.92       	  |
