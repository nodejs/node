'use strict';

const test = require('node:test');
const { setImmediate: setImmediatePromise } = require('node:timers/promises');
const executionOrder = [];

for (const name of ['a', 'b', 'c', 'd', 'e']) {
  test(`internal ${name}`, async () => {
    executionOrder.push(name);
    await setImmediatePromise();
  });
}

process.on('exit', () => {
  process.stdout.write(`EXECUTION_ORDER:${executionOrder.join(',')}\n`);
});
