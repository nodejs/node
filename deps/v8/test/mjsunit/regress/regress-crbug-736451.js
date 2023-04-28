// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string

!function() {
  const s0 = "external string turned into two byte";
  const s1 = s0.substring(1);
  externalizeString(s0, true);

  s1.toLowerCase();
}();
