// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(/^a[^a]$/u.test("a\u{1F310}"));
assertTrue(/^a[^a]$/u.test("a\u{1F310}"));
