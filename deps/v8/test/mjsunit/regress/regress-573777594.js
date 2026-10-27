// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const match = /^[z]?z|[38]/v.exec('aa3aaaaaaaaaaaaaaa');
assertNotNull(match);
assertEquals(2, match.index);
assertEquals('3', match[0]);
