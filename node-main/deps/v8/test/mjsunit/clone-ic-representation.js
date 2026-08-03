// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

// Ensure the clone ic correctly deals with representation change and map
// deprecation.

function Input(a, b) {
  this.a = a;
  this.b = b;
}

var input = new Input(1,1);
var input = new Input(1,1);
var exp = 2;

function ObjectSpread() {
  const result = { ...input, a: input.a };
  if (Object.values(result).reduce((a, b) => a + b) != exp) throw 666;
  return result
}

// Warmup
ObjectSpread()
ObjectSpread()
ObjectSpread()
ObjectSpread()

// Representation change
input = new Input(1,null);
var exp = 1;
ObjectSpread()
ObjectSpread()
ObjectSpread()
ObjectSpread()

// Map deprecation
input = new Input(1,1.4);
exp = 2.4;
ObjectSpread()
ObjectSpread()
ObjectSpread()
ObjectSpread()
