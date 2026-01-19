'use strict';

const common = require('../common');
const assert = require('node:assert');
const util = require('node:util');
const { WriteStream } = require('node:tty');

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
].forEach((invalidOption) => {
  assert.throws(() => {
    util.styleText(invalidOption, 'test');
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /grey/, // gray alias
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

assert.strictEqual(
  util.styleText('red', 'test', { validateStream: false }),
  '\u001b[31mtest\u001b[39m',
);

assert.strictEqual(
  util.styleText(['bold', 'red'], 'test', { validateStream: false }),
  '\u001b[1m\u001b[31mtest\u001b[39m\u001b[22m',
);

assert.strictEqual(
  util.styleText('red',
                 'A' + util.styleText('blue', 'B', { validateStream: false }) + 'C',
                 { validateStream: false }),
  '\u001b[31mA\u001b[34mB\u001b[31mC\u001b[39m'
);

assert.strictEqual(
  util.styleText('red',
                 'red' +
    util.styleText('blue', 'blue', { validateStream: false }) +
    'red' +
    util.styleText('blue', 'blue', { validateStream: false }) +
    'red',
                 { validateStream: false }
  ),
  '\x1B[31mred\x1B[34mblue\x1B[31mred\x1B[34mblue\x1B[31mred\x1B[39m'
);

assert.strictEqual(
  util.styleText('red',
                 'red' +
    util.styleText('blue', 'blue', { validateStream: false }) +
    'red' +
    util.styleText('red', 'red', { validateStream: false }) +
    'red' +
    util.styleText('blue', 'blue', { validateStream: false }),
                 { validateStream: false }
  ),
  '\x1b[31mred\x1b[34mblue\x1b[31mred\x1b[31mred\x1b[31mred\x1b[34mblue\x1b[39m\x1b[39m'
);

assert.strictEqual(
  util.styleText('red',
                 'A' + util.styleText(['bgRed', 'blue'], 'B', { validateStream: false }) +
    'C', { validateStream: false }),
  '\x1B[31mA\x1B[41m\x1B[34mB\x1B[31m\x1B[49mC\x1B[39m'
);

assert.strictEqual(
  util.styleText('dim',
                 'dim' +
    util.styleText('bold', 'bold', { validateStream: false }) +
  'dim', { validateStream: false }),
  '\x1B[2mdim\x1B[1mbold\x1B[22m\x1B[2mdim\x1B[22m'
);

assert.strictEqual(
  util.styleText('blue',
                 'blue' +
    util.styleText('red',
                   'red' +
      util.styleText('green', 'green', { validateStream: false }) +
      'red', { validateStream: false }) +
    'blue', { validateStream: false }),
  '\x1B[34mblue\x1B[31mred\x1B[32mgreen\x1B[31mred\x1B[34mblue\x1B[39m'
);

assert.strictEqual(
  util.styleText(
    'red',
    'red' +
    util.styleText(
      'blue',
      'blue' + util.styleText('red', 'red', {
        validateStream: false,
      }) + 'blue',
      {
        validateStream: false,
      }
    ) + 'red', {
      validateStream: false,
    }
  ),
  '\x1b[31mred\x1b[34mblue\x1b[31mred\x1b[34mblue\x1b[31mred\x1b[39m'
);

assert.strictEqual(
  util.styleText(['bold', 'red'], 'test', { validateStream: false }),
  util.styleText(
    'bold',
    util.styleText('red', 'test', { validateStream: false }),
    { validateStream: false },
  ),
);

assert.throws(() => {
  util.styleText(['invalid'], 'text');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

assert.throws(() => {
  util.styleText('red', 'text', { stream: {} });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});

// does not throw
util.styleText('red', 'text', { stream: {}, validateStream: false });

assert.strictEqual(
  util.styleText('red', 'test', { validateStream: false }),
  styled,
);

assert.strictEqual(util.styleText('none', 'test'), 'test');

const fd = common.getTTYfd();
if (fd !== -1) {
  const writeStream = new WriteStream(fd);

  const originalEnv = process.env;
  [
    { isTTY: true, env: {}, expected: styled },
    { isTTY: false, env: {}, expected: noChange },
    { isTTY: true, env: { NODE_DISABLE_COLORS: '1' }, expected: noChange },
    { isTTY: true, env: { NO_COLOR: '1' }, expected: noChange },
    { isTTY: true, env: { FORCE_COLOR: '1' }, expected: styled },
    { isTTY: true, env: { FORCE_COLOR: '1', NODE_DISABLE_COLORS: '1' }, expected: styled },
    { isTTY: false, env: { FORCE_COLOR: '1', NO_COLOR: '1', NODE_DISABLE_COLORS: '1' }, expected: styled },
    { isTTY: true, env: { FORCE_COLOR: '1', NO_COLOR: '1', NODE_DISABLE_COLORS: '1' }, expected: styled },
  ].forEach((testCase) => {
    writeStream.isTTY = testCase.isTTY;
    process.env = {
      ...process.env,
      ...testCase.env
    };
    {
      const output = util.styleText('red', 'test', { stream: writeStream });
      assert.strictEqual(output, testCase.expected);
    }
    {
      // Check that when passing an array of styles, the output behaves the same
      const output = util.styleText(['red'], 'test', { stream: writeStream });
      assert.strictEqual(output, testCase.expected);
    }
    process.env = originalEnv;
  });
} else {
  common.skip('Could not create TTY fd');
}
