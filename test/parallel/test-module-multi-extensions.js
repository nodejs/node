'use strict';

// Refs: https://github.com/nodejs/node/issues/4778

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const Module = require('module');
const tmpdir = require('../common/tmpdir');
const file = tmpdir.resolve('test-extensions.foo.bar');
const dotfile = tmpdir.resolve('.bar');
const dotfileWithExtension = tmpdir.resolve('.foo.bar');

tmpdir.refresh();
fs.writeFileSync(file, 'console.log(__filename);', 'utf8');
fs.writeFileSync(dotfile, 'console.log(__filename);', 'utf8');
fs.writeFileSync(dotfileWithExtension, 'console.log(__filename);', 'utf8');

{
  require.extensions['.bar'] = common.mustNotCall();
  require.extensions['.foo.bar'] = common.mustCall();
  const modulePath = tmpdir.resolve('test-extensions');
  require(modulePath);
  require(file);
  delete require.cache[file];
  delete require.extensions['.bar'];
  delete require.extensions['.foo.bar'];
  Module._pathCache = { __proto__: null };
}

{
  require.extensions['.foo.bar'] = common.mustCall();
  const modulePath = tmpdir.resolve('test-extensions');
  require(modulePath);
  assert.throws(
    () => require(`${modulePath}.foo`),
    (err) => err.message.startsWith(`Cannot find module '${modulePath}.foo'`)
  );
  require(`${modulePath}.foo.bar`);
  delete require.cache[file];
  delete require.extensions['.foo.bar'];
  Module._pathCache = { __proto__: null };
}

{
  const modulePath = tmpdir.resolve('test-extensions');
  assert.throws(
    () => require(modulePath),
    (err) => err.message.startsWith(`Cannot find module '${modulePath}'`)
  );
  delete require.cache[file];
  Module._pathCache = { __proto__: null };
}

{
  require.extensions['.bar'] = common.mustNotCall();
  require.extensions['.foo.bar'] = common.mustCall();
  const modulePath = tmpdir.resolve('test-extensions.foo');
  require(modulePath);
  delete require.cache[file];
  delete require.extensions['.bar'];
  delete require.extensions['.foo.bar'];
  Module._pathCache = { __proto__: null };
}

{
  require.extensions['.foo.bar'] = common.mustNotCall();
  const modulePath = tmpdir.resolve('test-extensions.foo');
  assert.throws(
    () => require(modulePath),
    (err) => err.message.startsWith(`Cannot find module '${modulePath}'`)
  );
  delete require.extensions['.foo.bar'];
  Module._pathCache = { __proto__: null };
}

{
  require.extensions['.bar'] = common.mustNotCall();
  require(dotfile);
  delete require.cache[dotfile];
  delete require.extensions['.bar'];
  Module._pathCache = { __proto__: null };
}

{
  require.extensions['.bar'] = common.mustCall();
  require.extensions['.foo.bar'] = common.mustNotCall();
  require(dotfileWithExtension);
  delete require.cache[dotfileWithExtension];
  delete require.extensions['.bar'];
  delete require.extensions['.foo.bar'];
  Module._pathCache = { __proto__: null };
}
