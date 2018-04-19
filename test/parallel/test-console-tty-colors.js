'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');
const { Writable } = require('stream');
const { Console } = require('console');

function check(isTTY, colorMode, expectedColorMode) {
  const items = [
    1,
    { a: 2 },
    [ 'foo' ],
    { '\\a': '\\bar' }
  ];

  let i = 0;
  const stream = new Writable({
    write: common.mustCall((chunk, enc, cb) => {
      assert.strictEqual(chunk.trim(),
                         util.inspect(items[i++], {
                           colors: expectedColorMode
                         }));
      cb();
    }, items.length),
    decodeStrings: false
  });
  stream.isTTY = isTTY;

  // Set ignoreErrors to `false` here so that we see assertion failures
  // from the `write()` call happen.
  const testConsole = new Console({
    stdout: stream,
    ignoreErrors: false,
    colorMode
  });
  for (const item of items) {
    testConsole.log(item);
  }
}

check(true, 'auto', true);
check(false, 'auto', false);
check(true, true, true);
check(false, true, true);
check(true, false, false);
check(false, false, false);
