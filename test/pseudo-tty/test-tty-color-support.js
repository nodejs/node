'use strict';

const common = require('../common');
const assert = require('assert').strict;
const { WriteStream } = require('tty');
const { inspect } = require('util');

const fd = common.getTTYfd();
const writeStream = new WriteStream(fd);

{
  const depth = writeStream.getColorDepth();
  assert.strictEqual(typeof depth, 'number');
  assert(depth >= 1 && depth <= 24);

  const support = writeStream.hasColors();
  assert.strictEqual(support, depth !== 1);
}

// Validate invalid input.
[true, null, () => {}, Symbol(), 5n].forEach((input) => {
  assert.throws(
    () => writeStream.hasColors(input),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

[-1, 1].forEach((input) => {
  assert.throws(
    () => writeStream.hasColors(input),
    { code: 'ERR_OUT_OF_RANGE' },
  );
});

// Check different environment variables.
[
  [{ COLORTERM: '1' }, 4],
  [{ COLORTERM: 'truecolor' }, 24],
  [{ COLORTERM: '24bit' }, 24],
  [{ TMUX: '1' }, 8],
  [{ CI: '1' }, 1],
  [{ CI: '1', TRAVIS: '1' }, 8],
  [{ CI: '1', CIRCLECI: '1' }, 8],
  [{ CI: '1', APPVEYOR: '1' }, 8],
  [{ CI: '1', GITLAB_CI: '1' }, 8],
  [{ CI: '1', CI_NAME: 'codeship' }, 8],
  [{ TEAMCITY_VERSION: '1.0.0' }, 1],
  [{ TEAMCITY_VERSION: '9.11.0' }, 4],
  [{ TERM_PROGRAM: 'iTerm.app' }, 8],
  [{ TERM_PROGRAM: 'iTerm.app', TERM_PROGRAM_VERSION: '3.0' }, 24],
  [{ TERM_PROGRAM: 'iTerm.app', TERM_PROGRAM_VERSION: '2.0' }, 8],
  [{ TERM_PROGRAM: 'HyperTerm' }, 24],
  [{ TERM_PROGRAM: 'Hyper' }, 1],
  [{ TERM_PROGRAM: 'MacTerm' }, 24],
  [{ TERM_PROGRAM: 'Apple_Terminal' }, 8],
  [{ TERM: 'xterm-256' }, 8],
  [{ TERM: 'ansi' }, 4],
  [{ TERM: 'ANSI' }, 4],
  [{ TERM: 'color' }, 4],
  [{ TERM: 'linux' }, 4],
  [{ TERM: 'fail' }, 1],
  [{ TERM: 'color', NODE_DISABLE_COLORS: '1' }, 1],
  [{ TERM: 'dumb' }, 1],
  [{ TERM: 'dumb', COLORTERM: '1' }, 1],
  [{ TERM: 'terminator' }, 24],
  [{ TERM: 'console' }, 4],
  [{ COLORTERM: '24bit', FORCE_COLOR: '' }, 4],
  [{ NO_COLOR: '1', FORCE_COLOR: '2' }, 8],
  [{ NODE_DISABLE_COLORS: '1', FORCE_COLOR: '3' }, 24],
  [{ NO_COLOR: '1', COLORTERM: '24bit' }, 1],
  [{ NO_COLOR: '', COLORTERM: '24bit' }, 1],
  [{ TMUX: '1', FORCE_COLOR: 0 }, 1],
  [{ NO_COLOR: 'true', FORCE_COLOR: 0, COLORTERM: 'truecolor' }, 1],
  [{ TERM: 'xterm-256color', COLORTERM: 'truecolor' }, 24],
].forEach(([env, depth], i) => {
  const actual = writeStream.getColorDepth(env);
  assert.strictEqual(
    actual,
    depth,
    `i: ${i}, expected: ${depth}, ` +
      `actual: ${actual}, env: ${inspect(env)}`,
  );
  const colors = 2 ** actual;
  assert(writeStream.hasColors(colors, env));
  assert(!writeStream.hasColors(colors + 1, env));
  assert(depth >= 4 ? writeStream.hasColors(env) : !writeStream.hasColors(env));
});

// OS settings
{
  const platform = Object.getOwnPropertyDescriptor(process, 'platform');
  const [ value, depth1, depth2 ] = process.platform !== 'win32' ?
    ['win32', 1, 4] : ['linux', 4, 1];

  assert.strictEqual(writeStream.getColorDepth({}), depth1);
  Object.defineProperty(process, 'platform', { value });
  assert.strictEqual(writeStream.getColorDepth({}), depth2);
  Object.defineProperty(process, 'platform', platform);
}
