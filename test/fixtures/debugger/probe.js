'use strict';

console.log('probe stdout');
console.error('probe stderr');

let total = 0;
for (let index = 0; index < 2; index++) {
  total += index + 40;
}

const finalValue = total;
console.log(finalValue);
