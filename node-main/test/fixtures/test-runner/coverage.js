'use strict';
const http = require('node:http'); // node:* should be filtered out.

try {
  // This block does not throw.
} catch { /* node:coverage ignore next 3 */
  // So this block is uncovered.

  /* node:coverage disable */

  /* node:coverage enable */

  /* node:coverage ignore next */
}

function uncalledTopLevelFunction() {
  if (true) {
    return 5;
  }

  return 9;
}

const test = require('node:test');

if (false) {
  console.log('this does not execute');
} else {
  require('./invalid-tap.js');
}

test('a test', () => {
  const uncalled = () => {};

  function fnWithControlFlow(val) {
    if (val < 0) {
      return -1;
    } else if (val === 0) {
      return 0;
    } else if (val < 100) {
      return 1;
    } else {
      return Infinity;
    }
  }

  fnWithControlFlow(-10);
  fnWithControlFlow(99);
});

try {
  require('test-nm'); // node_modules should be filtered out.
} catch {

}

async function main() {
  if (false) { console.log('boo'); } else { /* console.log('yay'); */ }

  if (false) {
    console.log('not happening');
  }

  if (true) {
    if (false) {
      console.log('not printed');
    }

    // Nothing to see here.
  } else {
    console.log('also not printed');
  }

  try {
    const foo = {};

    foo.x = 1;
    require('../v8-coverage/throw.js');
    foo.y = 2;
    return foo;
  } catch (err) {
    let bar = [];
    bar.push(5);
  } finally {
    const baz = 1;
  }
}

main();
