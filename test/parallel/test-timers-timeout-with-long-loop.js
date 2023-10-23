'use strict';

require('../common');

const assert = require('assert');

const orders = [];
process.on('exit', () => {
  assert.deepStrictEqual(orders, [ 1, 2, 3 ]);
});

setTimeout(() => {
  orders.push(1);
}, 10);

setTimeout(() => {
  orders.push(2);
}, 15);

let now = Date.now();
while (Date.now() - now < 100) {
  //
}

setTimeout(() => {
  orders.push(3);
}, 10);

now = Date.now();
while (Date.now() - now < 100) {
  //
}
