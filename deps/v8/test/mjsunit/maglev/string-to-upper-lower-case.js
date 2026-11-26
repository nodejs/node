// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --expose-externalize-string

const ext = createExternalizableString('Hello, WORLD!');
externalizeString(ext);

function test(toLower, optimize, non_string, expected) {
  %PrepareFunctionForOptimization(toLower);
  for (let i = 0; i < 10; i++) {
    assertEquals('abc', toLower('ABC'));
    assertEquals('hello, world!', toLower(ext));
  }

  optimize(toLower);

  assertEquals('abc', toLower('ABC'));
  assertEquals('', toLower(''));
  assertEquals('hello, world!', toLower(ext));
  assertEquals('\u03c9\u03b1', toLower('\u03a9\u0391'));
  if (typeof Intl !== 'undefined') {
    assertEquals('i\u0307', toLower('\u0130'));
  }
  assertOptimized(toLower);

  // Non-string receiver deopts cleanly.
  assertEquals(expected, toLower(non_string));
  assertUnoptimized(toLower);
}

const custom = { toLowerCase() { return 'custom'; } };
const optimizeMaglev = f => %OptimizeMaglevOnNextCall(f);
const optimizeTopTier = f => %OptimizeFunctionOnNextCall(f);

// Separate functions so that the tiers do not share feedback.
test(function(str) { return str.toLowerCase(); }, optimizeMaglev, custom,
     'custom');
test(function(str) { return str.toLowerCase(); }, optimizeTopTier, custom,
     'custom');

// The receiver is not covered by a map check here.
test(function(str) { return String.prototype.toLowerCase.call(str); },
     optimizeMaglev, 42, '42');
test(function(str) { return String.prototype.toLowerCase.call(str); },
     optimizeTopTier, { toString() { return 'OBJ'; } }, 'obj');

function testUpper(toUpper, optimize, non_string, expected) {
  %PrepareFunctionForOptimization(toUpper);
  for (let i = 0; i < 10; i++) {
    assertEquals('ABC', toUpper('abc'));
    assertEquals('HELLO, WORLD!', toUpper(ext));
  }

  optimize(toUpper);

  assertEquals('ABC', toUpper('abc'));
  assertEquals('', toUpper(''));
  assertEquals('HELLO, WORLD!', toUpper(ext));
  assertEquals('\u03a9\u0391', toUpper('\u03c9\u03b1'));
  if (typeof Intl !== 'undefined') {
    assertEquals('SS', toUpper('\u00df'));
  }
  assertOptimized(toUpper);

  // Non-string receiver deopts cleanly.
  assertEquals(expected, toUpper(non_string));
  assertUnoptimized(toUpper);
}

const customUpper = { toUpperCase() { return 'CUSTOM'; } };

testUpper(function(str) { return str.toUpperCase(); }, optimizeMaglev,
          customUpper, 'CUSTOM');
testUpper(function(str) { return str.toUpperCase(); }, optimizeTopTier,
          customUpper, 'CUSTOM');

testUpper(function(str) { return String.prototype.toUpperCase.call(str); },
          optimizeMaglev, 42, '42');
testUpper(function(str) { return String.prototype.toUpperCase.call(str); },
          optimizeTopTier, { toString() { return 'obj'; } }, 'OBJ');
