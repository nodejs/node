// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-staging

assertThrows(() => Function('using x'), SyntaxError);
assertThrows(() => Function('using x, y = null'), SyntaxError);
assertThrows(() => Function('await using x'), SyntaxError);
assertThrows(() => Function('await using x, y = null'), SyntaxError);
