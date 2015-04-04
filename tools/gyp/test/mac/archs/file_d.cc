// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "file_a.h"
#include "file_b.h"

void PublicFunctionD() {
  DependentFunctionA();
  DependentFunctionB();
}
