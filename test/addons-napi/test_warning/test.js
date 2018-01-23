'use strict';

if (process.argv[2] === 'child') {
  const common = require('../../common');
  console.log(require(`./build/${common.buildType}/test_warning`));
  console.log(require(`./build/${common.buildType}/test_warning2`));
} else {
  const run = require('child_process').spawnSync;
  const assert = require('assert');
  const warning = 'Warning: N-API is an experimental feature and could ' +
                  'change at any time.';

  const result = run(process.execPath, [__filename, 'child']);
  assert.deepStrictEqual(result.stdout.toString().match(/\S+/g), ['42', '1337']);
  assert.deepStrictEqual(result.stderr.toString().split(warning).length, 2);
}
