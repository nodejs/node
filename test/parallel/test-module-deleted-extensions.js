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

{
  require.extensions['.bar'] = common.mustNotCall();
  require.extensions['.foo.bar'] = common.mustNotCall();
  const modulePath = path.join(tmpdir.path, 'test-extensions');
  assert.throws(
    () => require(modulePath),
    new Error(`Cannot find module '${modulePath}'`)
  );
}

{
  require.extensions['.bar'] = common.mustNotCall();
  require.extensions['.foo.bar'] = common.mustNotCall();
  delete require.extensions['.bar'];
  const modulePath = path.join(tmpdir.path, 'test-extensions');
  assert.throws(
    () => require(modulePath),
    new Error(`Cannot find module '${modulePath}'`)
  );
}

{
  require.extensions['.foo.bar'] = common.mustNotCall();
  delete require.extensions['.foo.bar'];
  const modulePath = path.join(tmpdir.path, 'test-extensions');
  assert.throws(
    () => require(modulePath),
    new Error(`Cannot find module '${modulePath}'`)
  );
}

{
  require.extensions['.bar'] = common.mustCall((module, path) => {
    assert.strictEqual(module.id, file);
    assert.strictEqual(path, file);
  });

  const modulePath = path.join(tmpdir.path, 'test-extensions.foo');
  require(modulePath);
}
