# Quickbrot: Quickly rendering the mandelbrot set

Quickbrot is a multithreaded and vectorized renderer for the mandelbrot set written in C++. Uses AVX2 intrinsics to calculate 8 pixels in parallel per thread. Automatically divides the image into multiple threads. The result is a renderer that is very fast (A 1080p render of the region x:[-2.5,1], y:[-1,1] with 256 maximum iterations takes only ~40 ms on a 2 core 4 thread core i5-6200u laptop running Kubuntu 19.04). Uses the EasyBMP header library to create BMP image.
## Compile and Use
```bash
# Compile with 
g++ -O3 -pthread -mavx2 quickbrot.cc -o qbrot

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
  ### C++ Multithreading
  To completely parallelize the render, the program divides the image into sections and assigns a section to a single thread. A section contains >= 1 horizontal lines to render. A single line is rendered 8 pixels at a time. So on a 4 thread CPU, 32 pixels are being rendered at a time. 
  ```C++
  // pars is number of threads to use
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
  ```

  ## Render Times
  The timings shown below were measured on a 2 core 4 thread core i5-6200U laptop CPU running Kubuntu 19.04 with 8GB RAM. Scala implementation is multithreaded whereas C++ implementation is vectorized and multithreaded.
  
  
|  Resolution 	| Max Iterations 	| Scala Frame Time  (ms) 	| C++ Frame Time (ms) 	| Speedup 	|
|---------------|-----------------|-------------------------|-----------------------|-----------|
| 1920 x 1080 	|       64       	|         281.99         	|        14.24        	|   19.8  	|
| 1920 x 1080 	|       256      	|         441.32         	|        39.67        	|   11.1  	|
| 1920 x 1080 	|      1024      	|         1073.49        	|        136.34       	|   7.87  	|
