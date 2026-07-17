// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export default 42;
export let a = 1;
export {a as b};
export function set_a(x) { a = x };
export function get_a() { return a };
