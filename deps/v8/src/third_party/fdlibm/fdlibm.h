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

#ifndef V8_FDLIBM_H_
#define V8_FDLIBM_H_

namespace v8 {
namespace fdlibm {

int rempio2(double x, double* y);

// Constants to be exposed to builtins via Float64Array.
struct MathConstants {
  static const double constants[66];
};
}
}  // namespace v8::internal

#endif  // V8_FDLIBM_H_
