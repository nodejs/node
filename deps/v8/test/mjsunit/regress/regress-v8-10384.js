// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const s = "x";

assertEquals(/(?<\ud835\udc9c>.)/.exec(s).groups["\u{1d49c}"], s);
assertEquals(/(?<\ud835\udc9c>.)/u.exec(s).groups["\u{1d49c}"], s);
assertEquals(/(?<\u{1d49c}>.)/.exec(s).groups["\u{1d49c}"], s);
assertEquals(/(?<\u{1d49c}>.)/u.exec(s).groups["\u{1d49c}"], s);
assertEquals(/(?<𝒜>.)/.exec(s).groups["\u{1d49c}"], s);
assertEquals(/(?<𝒜>.)/u.exec(s).groups["\u{1d49c}"], s);
