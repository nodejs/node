// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var s = 'abcabcabcabcabc["possibly a sliced string"]'.slice(15)
assertEquals(["possibly a sliced string"], JSON.parse(s));
