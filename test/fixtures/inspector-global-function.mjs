'use strict';
let invocations = 0;
const interval = setInterval(() => {}, 1000);

global.sum = function() {
  const a = 1;
  const b = 2;
  const c = a + b;
  clearInterval(interval);
  console.log(invocations++, c);
};

// NOTE(mmarchini): Calls console.log two times to ensure we loaded every
// internal module before pausing. See
// https://bugs.chromium.org/p/v8/issues/detail?id=10287.
console.log('Loading');
console.log('Ready!');
