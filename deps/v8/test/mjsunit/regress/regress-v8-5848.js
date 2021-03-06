// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inlineFromParser = 50 ** 50;

const i = 50;
const fromRuntimePowOp = i ** i;
const fromRuntimeMath = Math.pow(i, i);

// inlineFromParser === fromRuntimeOp === fromRuntimeMath

assertEquals(inlineFromParser, fromRuntimePowOp);
assertEquals(inlineFromParser - fromRuntimePowOp, 0);

assertEquals(inlineFromParser, fromRuntimeMath);
assertEquals(inlineFromParser - fromRuntimeMath, 0);
