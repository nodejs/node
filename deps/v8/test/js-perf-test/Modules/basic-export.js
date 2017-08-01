// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export let value = 0;
for (let i = 0; i < iterations; ++i) ++value;
if (value != iterations) throw value;
