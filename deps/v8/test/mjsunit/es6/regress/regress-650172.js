// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var iterator = [].entries().__proto__.__proto__[Symbol.iterator];
print(1/iterator(-1E-300));
