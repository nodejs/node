// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj = { ['\u{D83D}\u{DE0A}']: new Proxy([], {})};
assertEquals('{"\u{D83D}\u{DE0A}":[]}', JSON.stringify(obj));
