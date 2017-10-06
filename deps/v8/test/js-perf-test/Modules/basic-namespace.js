// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as m from "value.js";
for (let i = 0; i < iterations; ++i) m.set(m.value + 1);
if (m.value != iterations) throw m.value;
m.set(0);
