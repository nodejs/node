// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var p = {};
var _ = {__proto__: p};
_ = null;
console.log(p.constructor.name);
a = { ...p};
p.asdf = 22;
a.asdfasdf = 22;
