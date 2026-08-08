// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pre = "(?:(?:" + "a".repeat(5461) + "){2}){3}";
const re = new RegExp("(?<=bb" + pre + ")c");
assertThrows(() => re.exec());
