// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var wrapper = new String('');
for (var i = 0; i < 100000; ++i) {
  '' + wrapper;
}
