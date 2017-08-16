'use strict';
// Refs: https://github.com/nodejs/node/issues/4778
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const file = path.join(common.tmpDir, 'test-extensions.foo.bar');

common.refreshTmpDir();
fs.writeFileSync(file, '', 'utf8');
require.extensions['.foo.bar'] = (module, path) => {};
delete require.extensions['.foo.bar'];
require.extensions['.bar'] = common.mustCall((module, path) => {
  assert.strictEqual(module.id, file);
  assert.strictEqual(path, file);
});
require(path.join(common.tmpDir, 'test-extensions'));
