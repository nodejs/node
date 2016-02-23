// The following is adapted from fdlibm (http://www.netlib.org/fdlibm).
//
// ====================================================
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
// ====================================================
//
// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2014 the V8 project authors. All rights reserved.

#include "src/third_party/fdlibm/fdlibm.h"

#include <stdint.h>
#include <cmath>
#include <limits>

#include "src/base/macros.h"
#include "src/double.h"

namespace v8 {
namespace fdlibm {

#ifdef _MSC_VER
inline double scalbn(double x, int y) { return _scalb(x, y); }
#endif  // _MSC_VER


// Table of constants for 2/pi, 396 Hex digits (476 decimal) of 2/pi
static const int two_over_pi[] = {
    0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C,
    0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A, 0x424DD2, 0xE00649,
    0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5, 0x2EBB44,
    0x84E99C, 0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C, 0x845F8B,
    0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D,
    0x367ECF, 0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
    0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330,
    0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E, 0xE61B08,
    0x659985, 0x5F14A0, 0x68408D, 0xFFD880, 0x4D7327, 0x310606, 0x1556CA,
    0x73A8C9, 0x60E27B, 0xC08C6B};

static const double zero = 0.0;
static const double two24 = 1.6777216e+07;
static const double one = 1.0;
static const double twon24 = 5.9604644775390625e-08;

static const double PIo2[] = {
    1.57079625129699707031e+00,  // 0x3FF921FB, 0x40000000
    7.54978941586159635335e-08,  // 0x3E74442D, 0x00000000
    5.39030252995776476554e-15,  // 0x3CF84698, 0x80000000
    3.28200341580791294123e-22,  // 0x3B78CC51, 0x60000000
    1.27065575308067607349e-29,  // 0x39F01B83, 0x80000000
    1.22933308981111328932e-36,  // 0x387A2520, 0x40000000
    2.73370053816464559624e-44,  // 0x36E38222, 0x80000000
    2.16741683877804819444e-51   // 0x3569F31D, 0x00000000
};


INLINE(int __kernel_rem_pio2(double* x, double* y, int e0, int nx)) {
  static const int32_t jk = 3;
  double fw;
  int32_t jx = nx - 1;
  int32_t jv = (e0 - 3) / 24;
  if (jv < 0) jv = 0;
  int32_t q0 = e0 - 24 * (jv + 1);
  int32_t m = jx + jk;

  double f[20];
  for (int i = 0, j = jv - jx; i <= m; i++, j++) {
    f[i] = (j < 0) ? zero : static_cast<double>(two_over_pi[j]);
  }

  double q[20];
  for (int i = 0; i <= jk; i++) {
    fw = 0.0;
    for (int j = 0; j <= jx; j++) fw += x[j] * f[jx + i - j];
    q[i] = fw;
  }

  int32_t jz = jk;

recompute:

  int32_t iq[20];
  double z = q[jz];
  for (int i = 0, j = jz; j > 0; i++, j--) {
    fw = static_cast<double>(static_cast<int32_t>(twon24 * z));
    iq[i] = static_cast<int32_t>(z - two24 * fw);
    z = q[j - 1] + fw;
  }

  z = scalbn(z, q0);
  z -= 8.0 * std::floor(z * 0.125);
  int32_t n = static_cast<int32_t>(z);
  z -= static_cast<double>(n);
  int32_t ih = 0;
  if (q0 > 0) {
    int32_t i = (iq[jz - 1] >> (24 - q0));
    n += i;
    iq[jz - 1] -= i << (24 - q0);
    ih = iq[jz - 1] >> (23 - q0);
  } else if (q0 == 0) {
    ih = iq[jz - 1] >> 23;
  } else if (z >= 0.5) {
    ih = 2;
  }

  if (ih > 0) {
    n += 1;
    int32_t carry = 0;
    for (int i = 0; i < jz; i++) {
      int32_t j = iq[i];
      if (carry == 0) {
        if (j != 0) {
          carry = 1;
          iq[i] = 0x1000000 - j;
        }
      } else {
        iq[i] = 0xffffff - j;
      }
    }
    if (q0 == 1) {
      iq[jz - 1] &= 0x7fffff;
    } else if (q0 == 2) {
      iq[jz - 1] &= 0x3fffff;
    }
    if (ih == 2) {
      z = one - z;
      if (carry != 0) z -= scalbn(one, q0);
    }
  }

  if (z == zero) {
    int32_t j = 0;
    for (int i = jz - 1; i >= jk; i--) j |= iq[i];
    if (j == 0) {
      int32_t k = 1;
      while (iq[jk - k] == 0) k++;
      for (int i = jz + 1; i <= jz + k; i++) {
        f[jx + i] = static_cast<double>(two_over_pi[jv + i]);
        for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
        q[i] = fw;
      }
      jz += k;
      goto recompute;
    }
  }

  if (z == 0.0) {
    jz -= 1;
    q0 -= 24;
    while (iq[jz] == 0) {
      jz--;
      q0 -= 24;
    }
  } else {
    z = scalbn(z, -q0);
    if (z >= two24) {
      fw = static_cast<double>(static_cast<int32_t>(twon24 * z));
      iq[jz] = static_cast<int32_t>(z - two24 * fw);
      jz += 1;
      q0 += 24;
      iq[jz] = static_cast<int32_t>(fw);
    } else {
      iq[jz] = static_cast<int32_t>(z);
    }
  }

  fw = scalbn(one, q0);
  for (int i = jz; i >= 0; i--) {
    q[i] = fw * static_cast<double>(iq[i]);
    fw *= twon24;
  }

  double fq[20];
  for (int i = jz; i >= 0; i--) {
    fw = 0.0;
    for (int k = 0; k <= jk && k <= jz - i; k++) fw += PIo2[k] * q[i + k];
    fq[jz - i] = fw;
  }

  fw = 0.0;
  for (int i = jz; i >= 0; i--) fw += fq[i];
  y[0] = (ih == 0) ? fw : -fw;
  fw = fq[0] - fw;
  for (int i = 1; i <= jz; i++) fw += fq[i];
  y[1] = (ih == 0) ? fw : -fw;
  return n & 7;
}


int rempio2(double x, double* y) {
  int32_t hx = static_cast<int32_t>(internal::double_to_uint64(x) >> 32);
  int32_t ix = hx & 0x7fffffff;

  if (ix >= 0x7ff00000) {
    *y = std::numeric_limits<double>::quiet_NaN();
    return 0;
  }

  int32_t e0 = (ix >> 20) - 1046;
  uint64_t zi = internal::double_to_uint64(x) & 0xFFFFFFFFu;
  zi |= static_cast<uint64_t>(ix - (e0 << 20)) << 32;
  double z = internal::uint64_to_double(zi);

  double tx[3];
  for (int i = 0; i < 2; i++) {
    tx[i] = static_cast<double>(static_cast<int32_t>(z));
    z = (z - tx[i]) * two24;
  }
  tx[2] = z;

  int nx = 3;
  while (tx[nx - 1] == zero) nx--;
  int n = __kernel_rem_pio2(tx, y, e0, nx);
  if (hx < 0) {
    y[0] = -y[0];
    y[1] = -y[1];
    return -n;
  }
  return n;
}
}  // namespace internal
}  // namespace v8
