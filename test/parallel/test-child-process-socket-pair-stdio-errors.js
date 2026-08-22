'use strict';
const assert = require('node:assert');
const { spawn, spawnSync } = require('node:child_process');
const { testCreateSocketPair } = require('../common/net-create-socket-pair');

const kSocketPairStdioMessage =
  /socket pair endpoints cannot be used as child process stdio/;

function assertSocketPairStdioThrows(stdio) {
  assert.throws(() => {
    spawn(process.execPath, ['-e', ''], { stdio });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    message: kSocketPairStdioMessage,
  });
}

testCreateSocketPair('socket pair endpoint is rejected as child stdin',
  (left, right) => {
    assertSocketPairStdioThrows([right, 'ignore', 'inherit']);

    left.end();
    right.end();
  });

testCreateSocketPair('socket pair endpoint is rejected as child stdout',
  (left, right) => {
    assertSocketPairStdioThrows(['ignore', right, 'inherit']);

    left.end();
    right.end();
  });

testCreateSocketPair('socket pair endpoint is rejected as child stderr',
  (left, right) => {
    assertSocketPairStdioThrows(['ignore', 'ignore', right]);

    left.end();
    right.end();
  });

testCreateSocketPair('socket pair endpoint is rejected as fd 3',
  (left, right) => {
    assertSocketPairStdioThrows(['ignore', 'ignore', 'inherit', right]);

    left.end();
    right.end();
  });

testCreateSocketPair('spawnSync rejects socket pair endpoint',
  (left, right) => {
    assert.throws(() => {
      spawnSync(process.execPath, ['-e', ''], {
        stdio: ['ignore', 'ignore', 'inherit', right],
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      message: kSocketPairStdioMessage,
    });

    left.end();
    right.end();
  });
