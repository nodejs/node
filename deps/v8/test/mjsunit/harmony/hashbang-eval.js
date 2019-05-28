// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-hashbang

// Hashbang syntax is allowed in eval.
assertEquals(eval("#!"), undefined);
assertEquals(eval("#!\n"), undefined);
assertEquals(eval("#!---IGNORED---"), undefined);
assertEquals(eval("#!---IGNORED---\n"), undefined);
