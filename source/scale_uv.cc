/*
 *  Copyright 2020 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "scale.h"

#include <assert.h>
#include <string.h>

#include "cpu_id.h"
#include "planar_functions.h"  // For CopyUV
#include "row.h"
#include "scale_row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Macros to enable specialized scalers

#ifndef HAS_SCALEUVDOWN2
#define HAS_SCALEUVDOWN2 1
#endif
#ifndef HAS_SCALEUVDOWN4BOX
#define HAS_SCALEUVDOWN4BOX 1
#endif
#ifndef HAS_SCALEUVDOWNEVEN
#define HAS_SCALEUVDOWNEVEN 1
#endif
#ifndef HAS_SCALEUVBILINEARDOWN
#define HAS_SCALEUVBILINEARDOWN 1
#endif
#ifndef HAS_SCALEUVBILINEARUP
#define HAS_SCALEUVBILINEARUP 1
#endif
#ifndef HAS_UVCOPY
#define HAS_UVCOPY 1
#endif
#ifndef HAS_SCALEPLANEVERTICAL
#define HAS_SCALEPLANEVERTICAL 1
#endif

static __inline int Abs(int v) {
  return v >= 0 ? v : -v;
}

// ScaleUV, 1/2
// This is an optimized version for scaling down a UV to 1/2 of
// its original size.
#if HAS_SCALEUVDOWN2
static void ScaleUVDown2(int src_width,
                         int src_height,
                         int dst_width,
                         int dst_height,
                         int src_stride,
                         int dst_stride,
                         const uint8_t* src_uv,
                         uint8_t* dst_uv,
                         int x,
                         int dx,
                         int y,
                         int dy,
                         enum FilterMode filtering) {
  int j;
  int row_stride = src_stride * (dy >> 16);
  void (*ScaleUVRowDown2)(const uint8_t* src_uv, ptrdiff_t src_stride,
                          uint8_t* dst_uv, int dst_width) =
      filtering == kFilterNone
          ? ScaleUVRowDown2_C
          : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_C
                                        : ScaleUVRowDown2Box_C);
  (void)src_width;
  (void)src_height;
  (void)dx;
  assert(dx == 65536 * 2);      // Test scale factor of 2.
  assert((dy & 0x1ffff) == 0);  // Test vertical scale is multiple of 2.
  // Advance to odd row, even column.
  if (filtering == kFilterBilinear) {
    src_uv += (y >> 16) * src_stride + (x >> 16) * 2;
  } else {
    src_uv += (y >> 16) * src_stride + ((x >> 16) - 1) * 2;
  }

#if defined(HAS_SCALEUVROWDOWN2BOX_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && filtering) {
    ScaleUVRowDown2 = ScaleUVRowDown2Box_Any_SSSE3;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVRowDown2 = ScaleUVRowDown2Box_SSSE3;
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWN2BOX_AVX2)
  if (TestCpuFlag(kCpuHasAVX2) && filtering) {
    ScaleUVRowDown2 = ScaleUVRowDown2Box_Any_AVX2;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVRowDown2 = ScaleUVRowDown2Box_AVX2;
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWN2BOX_NEON)
  if (TestCpuFlag(kCpuHasNEON) && filtering) {
    ScaleUVRowDown2 = ScaleUVRowDown2Box_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVRowDown2 = ScaleUVRowDown2Box_NEON;
    }
  }
#endif

// This code is not enabled.  Only box filter is available at this time.
#if defined(HAS_SCALEUVROWDOWN2_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ScaleUVRowDown2 =
        filtering == kFilterNone
            ? ScaleUVRowDown2_Any_SSSE3
            : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_Any_SSSE3
                                          : ScaleUVRowDown2Box_Any_SSSE3);
    if (IS_ALIGNED(dst_width, 2)) {
      ScaleUVRowDown2 =
          filtering == kFilterNone
              ? ScaleUVRowDown2_SSSE3
              : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_SSSE3
                                            : ScaleUVRowDown2Box_SSSE3);
    }
  }
#endif
// This code is not enabled.  Only box filter is available at this time.
#if defined(HAS_SCALEUVROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleUVRowDown2 =
        filtering == kFilterNone
            ? ScaleUVRowDown2_Any_NEON
            : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_Any_NEON
                                          : ScaleUVRowDown2Box_Any_NEON);
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVRowDown2 =
          filtering == kFilterNone
              ? ScaleUVRowDown2_NEON
              : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_NEON
                                            : ScaleUVRowDown2Box_NEON);
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWN2_MMI)
  if (TestCpuFlag(kCpuHasMMI)) {
    ScaleUVRowDown2 =
        filtering == kFilterNone
            ? ScaleUVRowDown2_Any_MMI
            : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_Any_MMI
                                          : ScaleUVRowDown2Box_Any_MMI);
    if (IS_ALIGNED(dst_width, 2)) {
      ScaleUVRowDown2 =
          filtering == kFilterNone
              ? ScaleUVRowDown2_MMI
              : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_MMI
                                            : ScaleUVRowDown2Box_MMI);
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWN2_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    ScaleUVRowDown2 =
        filtering == kFilterNone
            ? ScaleUVRowDown2_Any_MSA
            : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_Any_MSA
                                          : ScaleUVRowDown2Box_Any_MSA);
    if (IS_ALIGNED(dst_width, 2)) {
      ScaleUVRowDown2 =
          filtering == kFilterNone
              ? ScaleUVRowDown2_MSA
              : (filtering == kFilterLinear ? ScaleUVRowDown2Linear_MSA
                                            : ScaleUVRowDown2Box_MSA);
    }
  }
#endif

  if (filtering == kFilterLinear) {
    src_stride = 0;
  }
  for (j = 0; j < dst_height; ++j) {
    ScaleUVRowDown2(src_uv, src_stride, dst_uv, dst_width);
    src_uv += row_stride;
    dst_uv += dst_stride;
  }
}
#endif  // HAS_SCALEUVDOWN2

// ScaleUV, 1/4
// This is an optimized version for scaling down a UV to 1/4 of
// its original size.
#if HAS_SCALEUVDOWN4BOX
static void ScaleUVDown4Box(int src_width,
                            int src_height,
                            int dst_width,
                            int dst_height,
                            int src_stride,
                            int dst_stride,
                            const uint8_t* src_uv,
                            uint8_t* dst_uv,
                            int x,
                            int dx,
                            int y,
                            int dy) {
  int j;
  // Allocate 2 rows of UV.
  const int kRowSize = (dst_width * 2 * 2 + 15) & ~15;
  align_buffer_64(row, kRowSize * 2);
  int row_stride = src_stride * (dy >> 16);
  void (*ScaleUVRowDown2)(const uint8_t* src_uv, ptrdiff_t src_stride,
                          uint8_t* dst_uv, int dst_width) =
      ScaleUVRowDown2Box_C;
  // Advance to odd row, even column.
  src_uv += (y >> 16) * src_stride + (x >> 16) * 2;
  (void)src_width;
  (void)src_height;
  (void)dx;
  assert(dx == 65536 * 4);      // Test scale factor of 4.
  assert((dy & 0x3ffff) == 0);  // Test vertical scale is multiple of 4.

#if defined(HAS_SCALEUVROWDOWN2BOX_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ScaleUVRowDown2 = ScaleUVRowDown2Box_Any_SSSE3;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVRowDown2 = ScaleUVRowDown2Box_SSSE3;
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWN2BOX_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    ScaleUVRowDown2 = ScaleUVRowDown2Box_Any_AVX2;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVRowDown2 = ScaleUVRowDown2Box_AVX2;
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWN2BOX_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleUVRowDown2 = ScaleUVRowDown2Box_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVRowDown2 = ScaleUVRowDown2Box_NEON;
    }
  }
#endif

  for (j = 0; j < dst_height; ++j) {
    ScaleUVRowDown2(src_uv, src_stride, row, dst_width * 2);
    ScaleUVRowDown2(src_uv + src_stride * 2, src_stride, row + kRowSize,
                    dst_width * 2);
    ScaleUVRowDown2(row, kRowSize, dst_uv, dst_width);
    src_uv += row_stride;
    dst_uv += dst_stride;
  }
  free_aligned_buffer_64(row);
}
#endif  // HAS_SCALEUVDOWN4BOX

// ScaleUV Even
// This is an optimized version for scaling down a UV to even
// multiple of its original size.
#if HAS_SCALEUVDOWNEVEN
static void ScaleUVDownEven(int src_width,
                            int src_height,
                            int dst_width,
                            int dst_height,
                            int src_stride,
                            int dst_stride,
                            const uint8_t* src_uv,
                            uint8_t* dst_uv,
                            int x,
                            int dx,
                            int y,
                            int dy,
                            enum FilterMode filtering) {
  int j;
  int col_step = dx >> 16;
  int row_stride = (dy >> 16) * src_stride;
  void (*ScaleUVRowDownEven)(const uint8_t* src_uv, ptrdiff_t src_stride,
                             int src_step, uint8_t* dst_uv, int dst_width) =
      filtering ? ScaleUVRowDownEvenBox_C : ScaleUVRowDownEven_C;
  (void)src_width;
  (void)src_height;
  assert(IS_ALIGNED(src_width, 2));
  assert(IS_ALIGNED(src_height, 2));
  src_uv += (y >> 16) * src_stride + (x >> 16) * 2;
#if defined(HAS_SCALEUVROWDOWNEVEN_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ScaleUVRowDownEven = filtering ? ScaleUVRowDownEvenBox_Any_SSSE3
                                   : ScaleUVRowDownEven_Any_SSSE3;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVRowDownEven =
          filtering ? ScaleUVRowDownEvenBox_SSE2 : ScaleUVRowDownEven_SSSE3;
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWNEVEN_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleUVRowDownEven = filtering ? ScaleUVRowDownEvenBox_Any_NEON
                                   : ScaleUVRowDownEven_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVRowDownEven =
          filtering ? ScaleUVRowDownEvenBox_NEON : ScaleUVRowDownEven_NEON;
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWNEVEN_MMI)
  if (TestCpuFlag(kCpuHasMMI)) {
    ScaleUVRowDownEven =
        filtering ? ScaleUVRowDownEvenBox_Any_MMI : ScaleUVRowDownEven_Any_MMI;
    if (IS_ALIGNED(dst_width, 2)) {
      ScaleUVRowDownEven =
          filtering ? ScaleUVRowDownEvenBox_MMI : ScaleUVRowDownEven_MMI;
    }
  }
#endif
#if defined(HAS_SCALEUVROWDOWNEVEN_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    ScaleUVRowDownEven =
        filtering ? ScaleUVRowDownEvenBox_Any_MSA : ScaleUVRowDownEven_Any_MSA;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVRowDownEven =
          filtering ? ScaleUVRowDownEvenBox_MSA : ScaleUVRowDownEven_MSA;
    }
  }
#endif

  if (filtering == kFilterLinear) {
    src_stride = 0;
  }
  for (j = 0; j < dst_height; ++j) {
    ScaleUVRowDownEven(src_uv, src_stride, col_step, dst_uv, dst_width);
    src_uv += row_stride;
    dst_uv += dst_stride;
  }
}
#endif

// Scale UV down with bilinear interpolation.
#if HAS_SCALEUVBILINEARDOWN
static void ScaleUVBilinearDown(int src_width,
                                int src_height,
                                int dst_width,
                                int dst_height,
                                int src_stride,
                                int dst_stride,
                                const uint8_t* src_uv,
                                uint8_t* dst_uv,
                                int x,
                                int dx,
                                int y,
                                int dy,
                                enum FilterMode filtering) {
  int j;
  void (*InterpolateRow)(uint8_t * dst_uv, const uint8_t* src_uv,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) = InterpolateRow_C;
  void (*ScaleUVFilterCols)(uint8_t * dst_uv, const uint8_t* src_uv,
                            int dst_width, int x, int dx) =
      (src_width >= 32768) ? ScaleUVFilterCols64_C : ScaleUVFilterCols_C;
  int64_t xlast = x + (int64_t)(dst_width - 1) * dx;
  int64_t xl = (dx >= 0) ? x : xlast;
  int64_t xr = (dx >= 0) ? xlast : x;
  int clip_src_width;
  xl = (xl >> 16) & ~3;    // Left edge aligned.
  xr = (xr >> 16) + 1;     // Right most pixel used.  Bilinear uses 2 pixels.
  xr = (xr + 1 + 3) & ~3;  // 1 beyond 4 pixel aligned right most pixel.
  if (xr > src_width) {
    xr = src_width;
  }
  clip_src_width = (int)(xr - xl) * 2;  // Width aligned to 2.
  src_uv += xl * 2;
  x -= (int)(xl << 16);
#if defined(HAS_INTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    InterpolateRow = InterpolateRow_Any_SSSE3;
    if (IS_ALIGNED(clip_src_width, 16)) {
      InterpolateRow = InterpolateRow_SSSE3;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(clip_src_width, 32)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(clip_src_width, 16)) {
      InterpolateRow = InterpolateRow_NEON;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    InterpolateRow = InterpolateRow_Any_MSA;
    if (IS_ALIGNED(clip_src_width, 32)) {
      InterpolateRow = InterpolateRow_MSA;
    }
  }
#endif
#if defined(HAS_SCALEUVFILTERCOLS_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleUVFilterCols = ScaleUVFilterCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEUVFILTERCOLS_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleUVFilterCols = ScaleUVFilterCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVFilterCols = ScaleUVFilterCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEUVFILTERCOLS_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    ScaleUVFilterCols = ScaleUVFilterCols_Any_MSA;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVFilterCols = ScaleUVFilterCols_MSA;
    }
  }
#endif
  // TODO(fbarchard): Consider not allocating row buffer for kFilterLinear.
  // Allocate a row of UV.
  {
    align_buffer_64(row, clip_src_width * 2);

    const int max_y = (src_height - 1) << 16;
    if (y > max_y) {
      y = max_y;
    }
    for (j = 0; j < dst_height; ++j) {
      int yi = y >> 16;
      const uint8_t* src = src_uv + yi * src_stride;
      if (filtering == kFilterLinear) {
        ScaleUVFilterCols(dst_uv, src, dst_width, x, dx);
      } else {
        int yf = (y >> 8) & 255;
        InterpolateRow(row, src, src_stride, clip_src_width, yf);
        ScaleUVFilterCols(dst_uv, row, dst_width, x, dx);
      }
      dst_uv += dst_stride;
      y += dy;
      if (y > max_y) {
        y = max_y;
      }
    }
    free_aligned_buffer_64(row);
  }
}
#endif

// Scale UV up with bilinear interpolation.
#if HAS_SCALEUVBILINEARUP
static void ScaleUVBilinearUp(int src_width,
                              int src_height,
                              int dst_width,
                              int dst_height,
                              int src_stride,
                              int dst_stride,
                              const uint8_t* src_uv,
                              uint8_t* dst_uv,
                              int x,
                              int dx,
                              int y,
                              int dy,
                              enum FilterMode filtering) {
  int j;
  void (*InterpolateRow)(uint8_t * dst_uv, const uint8_t* src_uv,
                         ptrdiff_t src_stride, int dst_width,
                         int source_y_fraction) = InterpolateRow_C;
  void (*ScaleUVFilterCols)(uint8_t * dst_uv, const uint8_t* src_uv,
                            int dst_width, int x, int dx) =
      filtering ? ScaleUVFilterCols_C : ScaleUVCols_C;
  const int max_y = (src_height - 1) << 16;
#if defined(HAS_INTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    InterpolateRow = InterpolateRow_Any_SSSE3;
    if (IS_ALIGNED(dst_width, 4)) {
      InterpolateRow = InterpolateRow_SSSE3;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_AVX2)
  if (TestCpuFlag(kCpuHasAVX2)) {
    InterpolateRow = InterpolateRow_Any_AVX2;
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_AVX2;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    InterpolateRow = InterpolateRow_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      InterpolateRow = InterpolateRow_NEON;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_MMI)
  if (TestCpuFlag(kCpuHasMMI)) {
    InterpolateRow = InterpolateRow_Any_MMI;
    if (IS_ALIGNED(dst_width, 2)) {
      InterpolateRow = InterpolateRow_MMI;
    }
  }
#endif
#if defined(HAS_INTERPOLATEROW_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    InterpolateRow = InterpolateRow_Any_MSA;
    if (IS_ALIGNED(dst_width, 8)) {
      InterpolateRow = InterpolateRow_MSA;
    }
  }
#endif
  if (src_width >= 32768) {
    ScaleUVFilterCols = filtering ? ScaleUVFilterCols64_C : ScaleUVCols64_C;
  }
#if defined(HAS_SCALEUVFILTERCOLS_SSSE3)
  if (filtering && TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleUVFilterCols = ScaleUVFilterCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEUVFILTERCOLS_NEON)
  if (filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleUVFilterCols = ScaleUVFilterCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVFilterCols = ScaleUVFilterCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEUVFILTERCOLS_MSA)
  if (filtering && TestCpuFlag(kCpuHasMSA)) {
    ScaleUVFilterCols = ScaleUVFilterCols_Any_MSA;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVFilterCols = ScaleUVFilterCols_MSA;
    }
  }
#endif
#if defined(HAS_SCALEUVCOLS_SSSE3)
  if (!filtering && TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleUVFilterCols = ScaleUVCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEUVCOLS_NEON)
  if (!filtering && TestCpuFlag(kCpuHasNEON)) {
    ScaleUVFilterCols = ScaleUVCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVFilterCols = ScaleUVCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEUVCOLS_MMI)
  if (!filtering && TestCpuFlag(kCpuHasMMI)) {
    ScaleUVFilterCols = ScaleUVCols_Any_MMI;
    if (IS_ALIGNED(dst_width, 1)) {
      ScaleUVFilterCols = ScaleUVCols_MMI;
    }
  }
#endif
#if defined(HAS_SCALEUVCOLS_MSA)
  if (!filtering && TestCpuFlag(kCpuHasMSA)) {
    ScaleUVFilterCols = ScaleUVCols_Any_MSA;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVFilterCols = ScaleUVCols_MSA;
    }
  }
#endif
  if (!filtering && src_width * 2 == dst_width && x < 0x8000) {
    ScaleUVFilterCols = ScaleUVColsUp2_C;
#if defined(HAS_SCALEUVCOLSUP2_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(dst_width, 8)) {
      ScaleUVFilterCols = ScaleUVColsUp2_SSSE3;
    }
#endif
#if defined(HAS_SCALEUVCOLSUP2_MMI)
    if (TestCpuFlag(kCpuHasMMI) && IS_ALIGNED(dst_width, 4)) {
      ScaleUVFilterCols = ScaleUVColsUp2_MMI;
    }
#endif
  }

  if (y > max_y) {
    y = max_y;
  }

  {
    int yi = y >> 16;
    const uint8_t* src = src_uv + yi * src_stride;

    // Allocate 2 rows of UV.
    const int kRowSize = (dst_width * 2 + 15) & ~15;
    align_buffer_64(row, kRowSize * 2);

    uint8_t* rowptr = row;
    int rowstride = kRowSize;
    int lasty = yi;

    ScaleUVFilterCols(rowptr, src, dst_width, x, dx);
    if (src_height > 1) {
      src += src_stride;
    }
    ScaleUVFilterCols(rowptr + rowstride, src, dst_width, x, dx);
    src += src_stride;

    for (j = 0; j < dst_height; ++j) {
      yi = y >> 16;
      if (yi != lasty) {
        if (y > max_y) {
          y = max_y;
          yi = y >> 16;
          src = src_uv + yi * src_stride;
        }
        if (yi != lasty) {
          ScaleUVFilterCols(rowptr, src, dst_width, x, dx);
          rowptr += rowstride;
          rowstride = -rowstride;
          lasty = yi;
          src += src_stride;
        }
      }
      if (filtering == kFilterLinear) {
        InterpolateRow(dst_uv, rowptr, 0, dst_width * 2, 0);
      } else {
        int yf = (y >> 8) & 255;
        InterpolateRow(dst_uv, rowptr, rowstride, dst_width * 2, yf);
      }
      dst_uv += dst_stride;
      y += dy;
    }
    free_aligned_buffer_64(row);
  }
}
#endif  // HAS_SCALEUVBILINEARUP

// Scale UV to/from any dimensions, without interpolation.
// Fixed point math is used for performance: The upper 16 bits
// of x and dx is the integer part of the source position and
// the lower 16 bits are the fixed decimal part.

static void ScaleUVSimple(int src_width,
                          int src_height,
                          int dst_width,
                          int dst_height,
                          int src_stride,
                          int dst_stride,
                          const uint8_t* src_uv,
                          uint8_t* dst_uv,
                          int x,
                          int dx,
                          int y,
                          int dy) {
  int j;
  void (*ScaleUVCols)(uint8_t * dst_uv, const uint8_t* src_uv, int dst_width,
                      int x, int dx) =
      (src_width >= 32768) ? ScaleUVCols64_C : ScaleUVCols_C;
  (void)src_height;
#if defined(HAS_SCALEUVCOLS_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && src_width < 32768) {
    ScaleUVCols = ScaleUVCols_SSSE3;
  }
#endif
#if defined(HAS_SCALEUVCOLS_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ScaleUVCols = ScaleUVCols_Any_NEON;
    if (IS_ALIGNED(dst_width, 8)) {
      ScaleUVCols = ScaleUVCols_NEON;
    }
  }
#endif
#if defined(HAS_SCALEUVCOLS_MMI)
  if (TestCpuFlag(kCpuHasMMI)) {
    ScaleUVCols = ScaleUVCols_Any_MMI;
    if (IS_ALIGNED(dst_width, 1)) {
      ScaleUVCols = ScaleUVCols_MMI;
    }
  }
#endif
#if defined(HAS_SCALEUVCOLS_MSA)
  if (TestCpuFlag(kCpuHasMSA)) {
    ScaleUVCols = ScaleUVCols_Any_MSA;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleUVCols = ScaleUVCols_MSA;
    }
  }
#endif
  if (src_width * 2 == dst_width && x < 0x8000) {
    ScaleUVCols = ScaleUVColsUp2_C;
#if defined(HAS_SCALEUVCOLSUP2_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3) && IS_ALIGNED(dst_width, 8)) {
      ScaleUVCols = ScaleUVColsUp2_SSSE3;
    }
#endif
#if defined(HAS_SCALEUVCOLSUP2_MMI)
    if (TestCpuFlag(kCpuHasMMI) && IS_ALIGNED(dst_width, 4)) {
      ScaleUVCols = ScaleUVColsUp2_MMI;
    }
#endif
  }

  for (j = 0; j < dst_height; ++j) {
    ScaleUVCols(dst_uv, src_uv + (y >> 16) * src_stride, dst_width, x, dx);
    dst_uv += dst_stride;
    y += dy;
  }
}

// Copy UV with optional flipping
#if HAS_UVCOPY
static int UVCopy(const uint8_t* src_UV,
                  int src_stride_UV,
                  uint8_t* dst_UV,
                  int dst_stride_UV,
                  int width,
                  int height) {
  if (!src_UV || !dst_UV || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_UV = src_UV + (height - 1) * src_stride_UV;
    src_stride_UV = -src_stride_UV;
  }

  CopyPlane(src_UV, src_stride_UV, dst_UV, dst_stride_UV, width * 2, height);
  return 0;
}
#endif  // HAS_UVCOPY

// Scale a UV plane (from NV12)
// This function in turn calls a scaling function
// suitable for handling the desired resolutions.
static void ScaleUV(const uint8_t* src,
                    int src_stride,
                    int src_width,
                    int src_height,
                    uint8_t* dst,
                    int dst_stride,
                    int dst_width,
                    int dst_height,
                    int clip_x,
                    int clip_y,
                    int clip_width,
                    int clip_height,
                    enum FilterMode filtering) {
  // Initial source x/y coordinate and step values as 16.16 fixed point.
  int x = 0;
  int y = 0;
  int dx = 0;
  int dy = 0;
  // UV does not support box filter yet, but allow the user to pass it.
  // Simplify filtering when possible.
  filtering = ScaleFilterReduce(src_width, src_height, dst_width, dst_height,
                                filtering);

  // Negative src_height means invert the image.
  if (src_height < 0) {
    src_height = -src_height;
    src = src + (src_height - 1) * src_stride;
    src_stride = -src_stride;
  }
  ScaleSlope(src_width, src_height, dst_width, dst_height, filtering, &x, &y,
             &dx, &dy);
  src_width = Abs(src_width);
  if (clip_x) {
    int64_t clipf = (int64_t)(clip_x)*dx;
    x += (clipf & 0xffff);
    src += (clipf >> 16) * 2;
    dst += clip_x * 2;
  }
  if (clip_y) {
    int64_t clipf = (int64_t)(clip_y)*dy;
    y += (clipf & 0xffff);
    src += (clipf >> 16) * src_stride;
    dst += clip_y * dst_stride;
  }

  // Special case for integer step values.
  if (((dx | dy) & 0xffff) == 0) {
    if (!dx || !dy) {  // 1 pixel wide and/or tall.
      filtering = kFilterNone;
    } else {
      // Optimized even scale down. ie 2, 4, 6, 8, 10x.
      if (!(dx & 0x10000) && !(dy & 0x10000)) {
#if HAS_SCALEUVDOWN2
        if (dx == 0x20000) {
          // Optimized 1/2 downsample.
          ScaleUVDown2(src_width, src_height, clip_width, clip_height,
                       src_stride, dst_stride, src, dst, x, dx, y, dy,
                       filtering);
          return;
        }
#endif
#if HAS_SCALEUVDOWN4BOX
        if (dx == 0x40000 && filtering == kFilterBox) {
          // Optimized 1/4 box downsample.
          ScaleUVDown4Box(src_width, src_height, clip_width, clip_height,
                          src_stride, dst_stride, src, dst, x, dx, y, dy);
          return;
        }
#endif
#if HAS_SCALEUVDOWNEVEN
        ScaleUVDownEven(src_width, src_height, clip_width, clip_height,
                        src_stride, dst_stride, src, dst, x, dx, y, dy,
                        filtering);
        return;
#endif
      }
      // Optimized odd scale down. ie 3, 5, 7, 9x.
      if ((dx & 0x10000) && (dy & 0x10000)) {
        filtering = kFilterNone;
#ifdef HAS_UVCOPY
        if (dx == 0x10000 && dy == 0x10000) {
          // Straight copy.
          UVCopy(src + (y >> 16) * src_stride + (x >> 16) * 2, src_stride, dst,
                 dst_stride, clip_width, clip_height);
          return;
        }
#endif
      }
    }
  }
  // HAS_SCALEPLANEVERTICAL
  if (dx == 0x10000 && (x & 0xffff) == 0) {
    // Arbitrary scale vertically, but unscaled horizontally.
    ScalePlaneVertical(src_height, clip_width, clip_height, src_stride,
                       dst_stride, src, dst, x, y, dy, 4, filtering);
    return;
  }

#if HAS_SCALEUVBILINEARUP
  if (filtering && dy < 65536) {
    ScaleUVBilinearUp(src_width, src_height, clip_width, clip_height,
                      src_stride, dst_stride, src, dst, x, dx, y, dy,
                      filtering);
    return;
  }
#endif
#if HAS_SCALEUVBILINEARDOWN
  if (filtering) {
    ScaleUVBilinearDown(src_width, src_height, clip_width, clip_height,
                        src_stride, dst_stride, src, dst, x, dx, y, dy,
                        filtering);
    return;
  }
#endif
  ScaleUVSimple(src_width, src_height, clip_width, clip_height, src_stride,
                dst_stride, src, dst, x, dx, y, dy);
}

// Scale an UV image.
LIBYUV_API
int UVScale(const uint8_t* src_uv,
            int src_stride_uv,
            int src_width,
            int src_height,
            uint8_t* dst_uv,
            int dst_stride_uv,
            int dst_width,
            int dst_height,
            enum FilterMode filtering) {
  if (!src_uv || src_width == 0 || src_height == 0 || src_width > 32768 ||
      src_height > 32768 || !dst_uv || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  ScaleUV(src_uv, src_stride_uv, src_width, src_height, dst_uv, dst_stride_uv,
          dst_width, dst_height, 0, 0, dst_width, dst_height, filtering);
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
