// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let smiley = '\u{D83D}\u{DE0A}';
let obj1 = {[smiley]: undefined};
let obj2 = {[smiley]: {}};
assertEquals('{}', JSON.stringify(obj1));
assertEquals(`{"${smiley}":{}}`, JSON.stringify(obj2));
