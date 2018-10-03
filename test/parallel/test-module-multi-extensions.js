'use strict';

// Refs: https://github.com/nodejs/node/issues/4778

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const Module = require('module');
const tmpdir = require('../common/tmpdir');
const file = path.join(tmpdir.path, 'test-extensions.foo.bar');

tmpdir.refresh();
fs.writeFileSync(file, 'console.log(__filename);', 'utf8');

{
  require.extensions['.bar'] = common.mustNotCall();
  require.extensions['.foo.bar'] = common.mustCall();
  const modulePath = path.join(tmpdir.path, 'test-extensions');
  require(modulePath);
  require(file);
  delete require.cache[file];
  delete require.extensions['.bar'];
  delete require.extensions['.foo.bar'];
  Module._pathCache = Object.create(null);
}

{
  require.extensions['.foo.bar'] = common.mustCall();
  const modulePath = path.join(tmpdir.path, 'test-extensions');
  require(modulePath);
  assert.throws(
    () => require(`${modulePath}.foo`),
    new Error(`Cannot find module '${modulePath}.foo'`)
  );
  require(`${modulePath}.foo.bar`);
  delete require.cache[file];
  delete require.extensions['.foo.bar'];
  Module._pathCache = Object.create(null);
}

{
  const modulePath = path.join(tmpdir.path, 'test-extensions');
  assert.throws(
    () => require(modulePath),
    new Error(`Cannot find module '${modulePath}'`)
  );
  delete require.cache[file];
  Module._pathCache = Object.create(null);
}

{
  require.extensions['.bar'] = common.mustNotCall();
  require.extensions['.foo.bar'] = common.mustCall();
  const modulePath = path.join(tmpdir.path, 'test-extensions.foo');
  require(modulePath);
  delete require.cache[file];
  delete require.extensions['.bar'];
  delete require.extensions['.foo.bar'];
  Module._pathCache = Object.create(null);
}

{
  require.extensions['.foo.bar'] = common.mustNotCall();
  const modulePath = path.join(tmpdir.path, 'test-extensions.foo');
  assert.throws(
    () => require(modulePath),
    new Error(`Cannot find module '${modulePath}'`)
  );
  delete require.extensions['.foo.bar'];
  Module._pathCache = Object.create(null);
}
