/******************************************************************************
** Copyright (c) 2018, Intel Corporation                                     **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Hans Pabst (Intel Corp.)
******************************************************************************/
#include <libxsmm.h>
#include <stdio.h>

#if !defined(TYPE)
# define TYPE double
#endif
#if 0 /* No prefetch */
# define NOPREFETCH
#endif
#if 0 /* process batch of A, B, and C in "random" order */
# define SHUFFLE
#endif
#if 0 /* auto-dispatch SMM kernel */
# define AUTO
#endif


/**
 * Example program that multiplies matrices independently (C += A * B).
 * A and B-matrices are accumulated into C matrices (beta=1).
 * Streaming A, B, C, AB, AC, BC, or ABC are other useful benchmarks
 * However, running a kernel without loading any matrix operand from
 * memory ("cache-hot loop") is not modeling typical applications
 * since no actual work is performed.
 */
int main(int argc, char* argv[])
{
  /* batch-size is used to stream matrix-operands from memory */
  const int batchsize = (1 < argc ? atoi(argv[1]) : 0/*auto*/);
  /* default: M, N, and K are 13, 5, and 7 respectively */
  const int m = (2 < argc ? atoi(argv[2]) : 13);
  const int n = (3 < argc ? atoi(argv[3]) : 5);
  const int k = (4 < argc ? atoi(argv[4]) : 7);
  /* leading dimensions are made multiples of the size of a cache-line */
  const int lda = LIBXSMM_UP2(sizeof(TYPE) * m, LIBXSMM_CACHELINE) / sizeof(TYPE);
  const int ldb = LIBXSMM_UP2(sizeof(TYPE) * k, LIBXSMM_CACHELINE) / sizeof(TYPE);
  const int ldc = LIBXSMM_UP2(sizeof(TYPE) * m, LIBXSMM_CACHELINE) / sizeof(TYPE);
  /* micro-kernels are limited to certain alpha- and beta-values */
  const char transa = 'n', transb = 'n';
  const TYPE alpha = 1, beta = 1;
  /* calculate matrix sizes incl. padded elements */
  const size_t na = LIBXSMM_UP2(sizeof(TYPE) * lda * k, LIBXSMM_CACHELINE) / sizeof(TYPE);
  const size_t nb = LIBXSMM_UP2(sizeof(TYPE) * ldb * n, LIBXSMM_CACHELINE) / sizeof(TYPE);
  const size_t nc = LIBXSMM_UP2(sizeof(TYPE) * ldc * n, LIBXSMM_CACHELINE) / sizeof(TYPE);
  /* calculate default batch-size to hit work-set size of approx. 2 GB */
  const int size = (0 >= batchsize ? (int)((2ULL << 30/*2 GB*/) / (sizeof(TYPE) * (na + nb + nc))) : batchsize);
#if defined(SHUFFLE)
  const size_t shuffle = libxsmm_shuffle((unsigned int)size);
#endif
  /* allocate A, B, and C matrix buffers */
  TYPE *const a = (TYPE*)libxsmm_aligned_malloc(sizeof(TYPE) * na * size, LIBXSMM_CACHELINE);
  TYPE *const b = (TYPE*)libxsmm_aligned_malloc(sizeof(TYPE) * nb * size, LIBXSMM_CACHELINE);
  TYPE *const c = (TYPE*)libxsmm_aligned_malloc(sizeof(TYPE) * nc * size, LIBXSMM_CACHELINE);
  const double scale = 1.0 / size;
  libxsmm_timer_tickint start;
  double duration;
  int i, j;

  /**
   * LIBXSMM's C interface really is type-specific, and the helper macros (such as LIBXSMM_MMFUNCTION_TYPE)
   * are only for "entertainment". The C++ interface on the other hand is provides overloaded functions
   * and some helpers for more type-generic programming tasks (e.g., libxsmm_mmfunction<T>).
   */
#if !defined(AUTO) /* explicitly dispatch a kernel according to parameters */
  const int flags = LIBXSMM_GEMM_FLAGS(transa, transb), prefetch = LIBXSMM_PREFETCH_AUTO;
  LIBXSMM_MMFUNCTION_TYPE(TYPE) xmm = LIBXSMM_MMDISPATCH_SYMBOL(TYPE)(m, n, k, &lda, &ldb, &ldc, &alpha, &beta, &flags, &prefetch);
#endif

  /* initialize data according to touch-first policy */
#if defined(_OPENMP)
# pragma omp parallel for private(i, j)
#endif
  for (i = 0; i < size; ++i) {
#if defined(SHUFFLE)
    j = (i * shuffle) % size;
#else
    j = i;
#endif
    LIBXSMM_MATINIT(TYPE, 25 + i, a + j * na, m, k, lda, scale);
    LIBXSMM_MATINIT(TYPE, 75 + i, b + j * nb, k, n, ldb, scale);
    if (LIBXSMM_NEQ(0, beta)) { /* no need to initialize for beta=0 */
      LIBXSMM_MATINIT(TYPE, 42 + i, c + j * nc, m, n, ldc, scale);
    }
  }

#if defined(_OPENMP)
# pragma omp parallel
#endif
  {
#if !defined(_OPENMP)
    start = libxsmm_timer_tick();
#else /* OpenMP thread pool is already populated (parallel region) */
#   pragma omp single
    start = libxsmm_timer_tick();
#   pragma omp for private(i, j)
#endif
    for (i = 0; i < size - 1; ++i) {
#if defined(SHUFFLE)
# if !defined(AUTO) && !defined(NOPREFETCH)
      const int p = ((i + 1) * shuffle) % size;
# endif
      j = (i * shuffle) % size;
#else
# if !defined(AUTO) && !defined(NOPREFETCH)
      const int p = i + 1;
# endif
      j = i;
#endif
#if defined(AUTO)
      libxsmm_dgemm(&transa, &transb, &m, &n, &k,
        &alpha, a + j * na, &lda, b + j * nb, &ldb,
         &beta, c + j * nc, &ldc);
#elif defined(NOPREFETCH)
      xmm(a + j * na, b + j * nb, c + j * nc);
#else
      xmm(a + j * na, b + j * nb, c + j * nc,
          a + p * na, b + p * nb, c + p * nc);
#endif
    }
  }
#if defined(SHUFFLE)
  j = ((size - 1) * shuffle) % size;
#else
  j = size - 1;
#endif
#if defined(AUTO)
  libxsmm_dgemm(&transa, &transb, &m, &n, &k,
    &alpha, a + j * na, &lda, b + j * nb, &ldb,
     &beta, c + j * nc, &ldc);
#elif defined(NOPREFETCH)
  xmm(a + j * na, b + j * nb, c + j * nc);
#else
  xmm(a + j * na, b + j * nb, c + j * nc,
      a + j * na, b + j * nb, c + j * nc);
#endif
  duration = libxsmm_timer_duration(start, libxsmm_timer_tick());

  if (0 < duration) {
    const double gflops = 2.0 * m * n * k * 1E-9;
    printf("%.1f GFLOPS/s\n", gflops / duration * size);
  }
  printf("%.1f ms\n", 1000.0 * duration);

  libxsmm_free(a);
  libxsmm_free(b);
  libxsmm_free(c);

  return EXIT_SUCCESS;
}

