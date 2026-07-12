// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var re = /(?:abc\x7fd|abc\x80e)fghijklmnopq/i;
assertEquals(['abc\x7fdfghijklmnopq'], re.exec("abc\x7fdfghijklmnopq"));
assertEquals(['aBc\x80efghijklmnopq'], re.exec("aBc\x80efghijklmnopq"));
