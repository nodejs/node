// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>


new BenchmarkSuite('Spread-ES5', [1000], [
  new Benchmark('ES5', false, false, 0, ES5),
]);

new BenchmarkSuite('Spread-Traceur', [1000], [
  new Benchmark('Traceur', false, false, 0, Traceur),
]);

new BenchmarkSuite('Spread-ES6', [1000], [
  new Benchmark('ES6', false, false, 0, ES6),
]);

// ----------------------------------------------------------------------------
// Benchmark: ES5
// ----------------------------------------------------------------------------

function ES5() {
  "use strict";
  return Math.max.apply(Math, [1,2,3]);
}

// ----------------------------------------------------------------------------
// Benchmark: Traceur
// ----------------------------------------------------------------------------

function checkObjectCoercible(v) {
  "use strict";
  if (v === null || v === undefined) {
    throw new $TypeError('Value cannot be converted to an Object');
  }
  return v;
}

function spread() {
  "use strict";
  var rv = [],
      j = 0,
      iterResult;
  for (var i = 0; i < arguments.length; i++) {
    var valueToSpread = checkObjectCoercible(arguments[i]);
    if (typeof valueToSpread[Symbol.iterator] !== 'function') {
      throw new TypeError('Cannot spread non-iterable object.');
    }
    var iter = valueToSpread[Symbol.iterator]();
    while (!(iterResult = iter.next()).done) {
      rv[j++] = iterResult.value;
    }
  }
  return rv;
}

function Traceur() {
  "use strict";
  var $__0;
  return ($__0 = Math).max.apply($__0, spread([1, 2, 3]));
}

// ----------------------------------------------------------------------------
// Benchmark: ES6
// ----------------------------------------------------------------------------

function ES6() {
  "use strict";
  return Math.max(...[1,2,3]);
}
