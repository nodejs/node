'use strict';
// Refs: https://github.com/nodejs/node/issues/4778
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const file = path.join(tmpdir.path, 'test-extensions.foo.bar');

tmpdir.refresh();
fs.writeFileSync(file, '', 'utf8');
require.extensions['.foo.bar'] = (module, path) => {};
delete require.extensions['.foo.bar'];
require.extensions['.bar'] = common.mustCall((module, path) => {
  assert.strictEqual(module.id, file);
  assert.strictEqual(path, file);
});
require(path.join(tmpdir.path, 'test-extensions'));
