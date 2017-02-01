/******************************************************************************
** Copyright (c) 2016, Intel Corporation                                     **
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
/* Kunal Banerjee (Intel Corp.)
******************************************************************************/

  int total_tiles = handle->cwino_bwd.itiles*handle->cwino_bwd.jtiles;
  LIBXSMM_VLA_DECL(4, float, output, outp, handle->ifwp, handle->blocksifm, TDVLEN);
  LIBXSMM_VLA_DECL(5, float, input, toutp, ALPHA, (handle->blocksifm/VRATIO)*handle->cwino_bwd.bimg, total_tiles, FDVLEN);
  float Ow[total_tiles][ALPHA][ALPHA][FDVLEN];
  float O[ALPHA - 2][ALPHA - 2][FDVLEN];
  unsigned int ti, tj;
  int i, j, k, r;
  int xdim, ydim;
  float T[4][6][FDVLEN];
  float t0[FDVLEN];
  float t1[FDVLEN];
  float t2[FDVLEN];
  float t3[FDVLEN];

  for (j = 0; j < ALPHA; j++) {
    for (i = 0; i < ALPHA; i++) {
      for (tj = 0; tj < handle->cwino_bwd.jtiles; tj++) {
        for (ti = 0; ti < handle->cwino_bwd.itiles; ti++) {
          LIBXSMM_PRAGMA_SIMD
          for (k = 0; k < FDVLEN; k++) {
            Ow[tj*handle->cwino_bwd.itiles + ti][j][i][k] =
              LIBXSMM_VLA_ACCESS(5, input, j, i, 0, tj*handle->cwino_bwd.itiles + ti, k, ALPHA, (handle->blocksifm/VRATIO)*handle->cwino_bwd.bimg, total_tiles, FDVLEN);
          }
        }
      }
    }
  }
  for (tj = 0; tj < handle->cwino_bwd.jtiles; tj++) {
    for (ti = 0; ti < handle->cwino_bwd.itiles; ti++) {
      /*trans_O_4x4_3x3(ALPHA-2, FDVLEN, Ow[tj*handle->cwino_bwd.itiles + ti], O);*/

      /* inline code start */
      for (i = 0; i < 6; i++) {
        LIBXSMM_PRAGMA_SIMD
        for (j = 0; j < FDVLEN; j++) {
          t0[j] = Ow[tj*handle->cwino_bwd.itiles + ti][1][i][j] + Ow[tj*handle->cwino_bwd.itiles + ti][2][i][j];
          t1[j] = Ow[tj*handle->cwino_bwd.itiles + ti][3][i][j] + Ow[tj*handle->cwino_bwd.itiles + ti][4][i][j];
          t2[j] = Ow[tj*handle->cwino_bwd.itiles + ti][1][i][j] - Ow[tj*handle->cwino_bwd.itiles + ti][2][i][j];
          t3[j] = Ow[tj*handle->cwino_bwd.itiles + ti][3][i][j] - Ow[tj*handle->cwino_bwd.itiles + ti][4][i][j];

          T[0][i][j] = t0[j] + t1[j]     + Ow[tj*handle->cwino_bwd.itiles + ti][0][i][j];
          T[1][i][j] = t2[j] + t3[j]*2.0;
          T[2][i][j] = t0[j] + t1[j]*4.0;
          T[3][i][j] = t2[j] + t3[j]*8.0 + Ow[tj*handle->cwino_bwd.itiles + ti][5][i][j];
        }
      }

      for (i = 0; i < 4; i++) {
        LIBXSMM_PRAGMA_SIMD
        for (j = 0; j < FDVLEN; j++) {
          t0[j] = T[i][1][j] + T[i][2][j];
          t1[j] = T[i][3][j] + T[i][4][j];
          t2[j] = T[i][1][j] - T[i][2][j];
          t3[j] = T[i][3][j] - T[i][4][j];

          O[i][0][j] = t0[j] + t1[j]     + T[i][0][j];
          O[i][1][j] = t2[j] + t3[j]*2.0;
          O[i][2][j] = t0[j] + t1[j]*4.0;
          O[i][3][j] = t2[j] + t3[j]*8.0 + T[i][5][j];
        }
      }
      /* inline code end */

      for (j = 0; j < ALPHA-2; j++) {
        ydim = tj*(ALPHA - 2) + j;
        if (ydim >= handle->desc.H) continue;
        for (i = 0; i < ALPHA-2; i++) {
          xdim = ti*(ALPHA - 2) + i;
          if (xdim >= handle->desc.W) continue;
          for (r = 0; r < VRATIO; r++) {
            LIBXSMM_PRAGMA_SIMD
            for (k = 0; k < TDVLEN; k++) {
              LIBXSMM_VLA_ACCESS(4, output, ydim + handle->desc.pad_h, xdim + handle->desc.pad_w, r, k, handle->ifwp, handle->blocksifm, TDVLEN) +=
                O[j][i][r*TDVLEN + k];
            }
          }
        }
      }
    }
  }