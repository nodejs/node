// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// let is usable as a variable with var, but not let or ES6 const

(function (){
  assertEquals(undefined, let);

  var let;

  let = 5;
  assertEquals(5, let);

  (function() { var let = 1; assertEquals(1, let); })();
  assertEquals(5, let);
})();

assertThrows(function() { return let; }, ReferenceError);

(function() {
   var let, sum = 0;
   for (let in [1, 2, 3, 4]) sum += Number(let);
   assertEquals(6, sum);

   (function() { for (var let of [4, 5]) sum += let; })();
   assertEquals(15, sum);

   (function() { for (var let in [6]) sum += Number([6][let]); })();
   assertEquals(21, sum);

   for (let = 7; let < 8; let++) sum += let;
   assertEquals(28, sum);
   assertEquals(8, let);

   (function() { for (var let = 8; let < 9; let++) sum += let; })();
   assertEquals(36, sum);
   assertEquals(8, let);
})();

assertThrows(function() { return let; }, ReferenceError);

(function () {
  let obj = {};
  var {let} = {let() { return obj; }};
  let().x = 1;
  assertEquals(1, obj.x);
})();

(function() {
  function let() {
    return 1;
  }
  assertEquals(1, let());
})()

assertThrows('for (let of []) {}', SyntaxError);
