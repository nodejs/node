// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Math.alignUp = function(raw, multiple) {
  return Math.floor((raw + multiple - 1) / multiple) * multiple;
}
