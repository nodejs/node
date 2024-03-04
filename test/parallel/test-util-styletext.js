'use strict';
require('../common');
const assert = require('assert');
const util = require('util');
const { execSync } = require('child_process');
const { EOL } = require('os');
const styled = '\u001b[31mtest\u001b[39m';
const noChange = 'test';


[
  undefined,
  null,
  false,
  5n,
  5,
  Symbol(),
  () => {},
  {},
  [],
].forEach((invalidOption) => {
  assert.throws(() => {
    util.styleText(invalidOption, 'test');
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => {
    util.styleText('red', invalidOption);
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

assert.throws(() => {
  util.styleText('invalid', 'text');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

assert.strictEqual(util.styleText('red', 'test'), styled);

function checkCase(isTTY, env, expected) {
  process.stdout.isTTY = isTTY;
  const code = `
    process.stdout.isTTY = ${isTTY}; console.log(util.styleText('red', 'test'));
  `;
  const output = execSync(`${process.execPath} -e "${code}"`, { env }).toString();
  assert.strictEqual(output, expected + EOL);
}


[
  { isTTY: true, env: {}, expected: styled },
  { isTTY: false, env: {}, expected: noChange },
  { isTTY: true, env: { NODE_DISABLE_COLORS: 1 }, expected: noChange },
  { isTTY: true, env: { NO_COLOR: 1 }, expected: noChange },
  { isTTY: true, env: { FORCE_COLOR: 1 }, expected: styled },
  { isTTY: true, env: { FORCE_COLOR: 1, NODE_DISABLE_COLORS: 1 }, expected: styled },
  { isTTY: false, env: { FORCE_COLOR: 1, NO_COLOR: 1, NODE_DISABLE_COLORS: 1 }, expected: noChange },
  { isTTY: true, env: { FORCE_COLOR: 1, NO_COLOR: 1, NODE_DISABLE_COLORS: 1 }, expected: styled },
].forEach((testCase) => {
  checkCase(testCase.isTTY, testCase.env, testCase.expected);
});
