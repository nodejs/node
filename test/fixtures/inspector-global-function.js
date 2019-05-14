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

console.log('Ready!');
