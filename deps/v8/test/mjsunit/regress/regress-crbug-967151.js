// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string

__v_3 = "100                         external string turned into two byte";
__v_2 = __v_3.substring(0, 28);
try {
  externalizeString(__v_3, true);
} catch (e) {}
assertEquals(100, JSON.parse(__v_2));
