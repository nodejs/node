// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {value, set} from "value.js";
for (let i = 0; i < iterations; ++i) set(value + 1);
if (value != iterations) throw value;
set(0);
