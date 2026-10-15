// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(1, /[B]|.+?!^/s.exec('1B').index);
assertEquals(0, /(?s:.*?D)?^/.exec('').index);
assertEquals(0, /[^]+?w^|/.exec('a').index);
assertEquals(0, /.*?1^|/s.exec('A,A ').index);
assertEquals(0, /.+?](^)|/s.exec('0').index);
