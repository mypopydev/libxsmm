/******************************************************************************
** Copyright (c) 2017, Intel Corporation                                     **
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

const LIBXSMM_MATDIFF_TEMPLATE_ELEM_TYPE *const real_ref = (const LIBXSMM_MATDIFF_TEMPLATE_ELEM_TYPE*)ref;
const LIBXSMM_MATDIFF_TEMPLATE_ELEM_TYPE *const real_tst = (const LIBXSMM_MATDIFF_TEMPLATE_ELEM_TYPE*)tst;
double compf = 0, compfr = 0, compft = 0, normfr = 0, normft = 0, normr = 0, normc = 0;
libxsmm_blasint i, j;

for (i = 0; i < nn; ++i) {
  double comprj = 0, comptj = 0, compij = 0;
  double normrj = 0, normtj = 0, normij = 0;

  for (j = 0; j < mm; ++j) {
    const double ri = real_ref[i*ldr+j], ti = real_tst[i*ldt+j];
    const double di = (ri < ti ? (ti - ri) : (ri - ti));
    const double ra = LIBXSMM_ABS(ri);
    const double ta = LIBXSMM_ABS(ti);

    /* row-wise sum of reference values with Kahan compensation */
    double v0 = ra - comprj, v1 = normrj + v0;
    comprj = (v1 - normrj) - v0;
    normrj = v1;

    /* row-wise sum of test values with Kahan compensation */
    v0 = ta - comptj; v1 = normtj + v0;
    comptj = (v1 - normtj) - v0;
    normtj = v1;

    /* row-wise sum of differences with Kahan compensation */
    v0 = di - compij; v1 = normij + v0;
    compij = (v1 - normij) - v0;
    normij = v1;

    /* Froebenius-norm of reference matrix with Kahan compensation */
    v0 = ri * ri - compfr; v1 = normfr + v0;
    compfr = (v1 - normfr) - v0;
    normfr = v1;

    /* Froebenius-norm of test matrix with Kahan compensation */
    v0 = ti * ti - compft; v1 = normft + v0;
    compft = (v1 - normft) - v0;
    normft = v1;

    /* Froebenius-norm of differences with Kahan compensation */
    v0 = di * di - compf; v1 = info->normf_abs + v0;
    compf = (v1 - info->normf_abs) - v0;
    info->normf_abs = v1;
  }

  /* calculate Infinity-norm of differences */
  if (info->normi_abs < normij) info->normi_abs = normij;
  /* calculate Infinity-norm of reference/test values */
  if (normr < normrj) normr = normrj;
  if (normr < normtj) normr = normtj;
}

/* Infinity-norm relative to MAX(Infinity-norm-ref, Infinity-norm-test) */
if (0 < normr) {
  info->normi_rel = info->normi_abs / normr;
}
else { /* should not happen */
  info->normi_rel = info->normi_abs;
}

/* Froebenius-norm relative to MAX(F-norm-ref, F-norm-test) */
if (normfr < normft) normfr = normft;
if (0 < normfr) {
  info->normf_rel = info->normf_abs / normfr;
}
else { /* should not happen */
  info->normf_rel = info->normf_abs;
}

for (j = 0; j < mm; ++j) {
  double compri = 0, compti = 0, comp1 = 0;
  double normri = 0, normti = 0, norm1 = 0;

  for (i = 0; i < nn; ++i) {
    const double ri = real_ref[i*ldr+j], ti = real_tst[i*ldt+j];
    const double di = (ri < ti ? (ti - ri) : (ri - ti));
    const double ra = LIBXSMM_ABS(ri);
    const double ta = LIBXSMM_ABS(ti);

    /* column-wise sum of reference values with Kahan compensation */
    double v0 = ra - compri, v1 = normri + v0;
    compri = (v1 - normri) - v0;
    normri = v1;

    /* column-wise sum of test values with Kahan compensation */
    v0 = ta - compti; v1 = normti + v0;
    compti = (v1 - normti) - v0;
    normti = v1;

    /* column-wise sum of differences with Kahan compensation */
    v0 = di - comp1; v1 = norm1 + v0;
    comp1 = (v1 - norm1) - v0;
    norm1 = v1;
  }

  /* calculate One-norm of differences */
  if (info->norm1_abs < norm1) info->norm1_abs = norm1;
  /* calculate One-norm of reference/test values */
  if (normc < normri) normc = normri;
  if (normc < normti) normc = normti;
}

/* One-norm relative to MAX(One-norm-ref, One-norm-test) */
if (0 < normc) {
  info->norm1_rel = info->norm1_abs / normc;
}
else { /* should not happen */
  info->norm1_rel = info->norm1_abs;
}
