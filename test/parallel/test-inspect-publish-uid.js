'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');

(async function test() {
  await testArg('stderr');
  await testArg('http');
  await testArg('http,stderr');
})();

async function testArg(argValue) {
  console.log('Checks ' + argValue + '..');
  const hasHttp = argValue.split(',').includes('http');
  const hasStderr = argValue.split(',').includes('stderr');

  const nodeProcess = spawnSync(process.execPath, [
    '--inspect=0',
    `--inspect-publish-uid=${argValue}`,
    '-e', `(${scriptMain.toString()})(${hasHttp ? 200 : 404})`
  ]);
  const hasWebSocketInStderr = checkStdError(
    nodeProcess.stderr.toString('utf8'));
  assert.strictEqual(hasWebSocketInStderr, hasStderr);

  function checkStdError(data) {
    const matches = data.toString('utf8').match(/ws:\/\/.+:(\d+)\/.+/);
    return !!matches;
  }

  function scriptMain(code) {
    const url = require('inspector').url();
    const { host } = require('url').parse(url);
    require('http').get('http://' + host + '/json/list', (response) => {
      assert.strictEqual(response.statusCode, code);
      response.destroy();
    });
  }
}
