#include <immintrin.h>

using vec8f = __m256;
using vec8i = __m256i;

struct cvec8f {
  vec8f re, im;

  cvec8f() = default;

  inline static const int len() {
    return sizeof(vec8f) / sizeof(float);
  }

  inline cvec8f (float real) {
    re = _mm256_set1_ps(real);
    im = _mm256_setzero_ps();
  }

  inline cvec8f (float real, float imag) {
    re = _mm256_set1_ps(real);
    im = _mm256_set1_ps(imag);
  }

  inline cvec8f (float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7) {
    re = _mm256_setr_ps(f0, f1, f2, f3, f4, f5, f6, f7);
    im = _mm256_setzero_ps();
  }

  inline cvec8f (float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7, float imag) {
    re = _mm256_setr_ps(f0, f1, f2, f3, f4, f5, f6, f7);
    im = _mm256_set1_ps(imag);
  }

  inline cvec8f (float f0, float inc, float imag) {
    re = _mm256_add_ps(_mm256_set1_ps(f0), _mm256_setr_ps(0, inc, 2*inc, 3*inc, 4*inc, 5*inc, 6*inc, 7*inc));
    im = _mm256_set1_ps(imag);
  }

  inline cvec8f operator+(const cvec8f &left) {
    cvec8f res;
    res.re = re + left.re;
    res.im = im + left.im;
    return res;
  }

  inline cvec8f operator-(const cvec8f &left) {
    cvec8f res;
    res.re = re - left.re;
    res.im = im - left.im;
    return res;
  }

  inline cvec8f operator*(const cvec8f &left) {
    cvec8f res;
    res.re = (re * left.re) - (im * left.im);
    res.im = (im * left.re) + (re * left.im);
    return res;
  }

  inline vec8f mag() const {
    return re * re + im * im;
  }

  inline vec8i magLessThan(float limit) const {
    return _mm256_castps_si256(_mm256_cmp_ps(mag(), _mm256_set1_ps(limit), _CMP_LT_OQ));
  }
};

struct Counter8i {
  vec8i count;

  Counter8i () {
    count = _mm256_setzero_si256();
  }

  /**
   * Checks if the current count has reached the threshold. Returns true
   * if it does
   */
  inline vec8i equal(int max) {
    return _mm256_cmpeq_epi32(count, _mm256_set1_epi32(max));
  }

  /**
   * Increments the counter if mag in limit and count less 
   * than max. Returns true if needs another iteration
   */
  inline bool increment(int max, vec8i magInLimit) {
    // check which have reached the limit
    const auto eq = equal(max);

    // check which have not reached the limit (both mag and iteration)
    // -1 if hasnt else 0
    const auto inc = _mm256_andnot_si256(eq, magInLimit);
    
    // count - (-1) = count + 1
    count = _mm256_sub_epi32(count, inc);

    // check if all elements have reached their limits
    // if all limits are reached the intrinsic will return 1
    return _mm256_testc_si256(eq, magInLimit) == 0;
  }
};

inline void store(vec8i vec, int *location) {
  _mm256_store_si256((vec8i*) location, vec);
}