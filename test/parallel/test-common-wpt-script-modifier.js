'use strict';

const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawnSync } = require('child_process');

if (process.env.NODE_TEST_WPT_MODIFIER_PROBE === '1') {
  const { WPTRunner } = require('../common/wpt');
  const runner = new WPTRunner('html/webappapis/atob');
  runner.setScriptModifier(common.mustCall((script) => {
    assert.strictEqual(script.filename,
                       fixtures.path('wpt', 'html/webappapis/atob', 'base64.any.js'));
    script.filename += '.modified';
    script.code = `test(() => {
      assert_true(new Error().stack.includes(${JSON.stringify(`${script.filename}:`)}));
    }, 'modified script retains its filename');`;
  }));
  runner.runJsTests();
} else {
  // eslint-disable-next-line no-unused-vars
  const { WPT_REPORT, ...env } = { ...process.env, NODE_TEST_WPT_MODIFIER_PROBE: '1' };
  const result = spawnSync(process.execPath, [__filename, 'base64.any.js'], {
    env,
    encoding: 'utf8',
    timeout: common.platformTimeout(10_000),
  });
  const { error, status, stdout, stderr } = result;
  assert.ifError(error);
  assert.strictEqual(status, 0, stdout + stderr);
  const results = stdout.split('\n').filter((line) => line.startsWith('[PASS]'));
  assert.deepStrictEqual(results, [
    '[PASS] modified script retains its filename',
  ]);
}
