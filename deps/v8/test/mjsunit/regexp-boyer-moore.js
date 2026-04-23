// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function UseSpecialBytecode() {
  // Trigger the single-position Boyer-Moore optimization.
  var re = /.......a[^l]/;
  subject = 'Now is the time for all good men';
  for (var i = 0; i < 5; i++) subject += subject;
  for (var i = 0; i < 5; i++) {
    re.test(subject);
  }
}

UseSpecialBytecode();
