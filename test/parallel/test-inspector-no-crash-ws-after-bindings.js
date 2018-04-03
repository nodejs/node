// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.crashOnUnhandledRejection();
const { NodeInstance } = require('../common/inspector-helper.js');
const assert = require('assert');

const expected = 'Can connect now!';

const script = `
  'use strict';
  const { Session } = require('inspector');

  const s = new Session();
  s.connect();
  console.error('${expected}');
  process.stdin.on('data', () => process.exit(0));
`;

async function runTests() {
  const instance = new NodeInstance(['--inspect=0', '--expose-internals'],
                                    script);
  while (await instance.nextStderrString() !== expected);
  assert.strictEqual(400, await instance.expectConnectionDeclined());
  instance.write('Stop!\n');
  assert.deepStrictEqual({ exitCode: 0, signal: null },
                         await instance.expectShutdown());
}

runTests();
