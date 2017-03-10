// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

console.error('load test-module-loading.js');

// assert that this is the main module.
assert.strictEqual(require.main.id, '.', 'main module should have id of \'.\'');
assert.strictEqual(require.main, module, 'require.main should === module');
assert.strictEqual(process.mainModule, module,
                   'process.mainModule should === module');
// assert that it's *not* the main module in the required module.
require('../fixtures/not-main-module.js');

// require a file with a request that includes the extension
const a_js = require('../fixtures/a.js');
assert.strictEqual(42, a_js.number);

// require a file without any extensions
const foo_no_ext = require('../fixtures/foo');
assert.strictEqual('ok', foo_no_ext.foo);

const a = require('../fixtures/a');
const c = require('../fixtures/b/c');
const d = require('../fixtures/b/d');
const d2 = require('../fixtures/b/d');
// Absolute
const d3 = require(path.join(__dirname, '../fixtures/b/d'));
// Relative
const d4 = require('../fixtures/b/d');

assert.strictEqual(false, false, 'testing the test program.');

assert.ok(a.A instanceof Function);
assert.strictEqual('A', a.A());

assert.ok(a.C instanceof Function);
assert.strictEqual('C', a.C());

assert.ok(a.D instanceof Function);
assert.strictEqual('D', a.D());

assert.ok(d.D instanceof Function);
assert.strictEqual('D', d.D());

assert.ok(d2.D instanceof Function);
assert.strictEqual('D', d2.D());

assert.ok(d3.D instanceof Function);
assert.strictEqual('D', d3.D());

assert.ok(d4.D instanceof Function);
assert.strictEqual('D', d4.D());

assert.ok((new a.SomeClass()) instanceof c.SomeClass);

console.error('test index.js modules ids and relative loading');
const one = require('../fixtures/nested-index/one');
const two = require('../fixtures/nested-index/two');
assert.notStrictEqual(one.hello, two.hello);

console.error('test index.js in a folder with a trailing slash');
const three = require('../fixtures/nested-index/three');
const threeFolder = require('../fixtures/nested-index/three/');
const threeIndex = require('../fixtures/nested-index/three/index.js');
assert.strictEqual(threeFolder, threeIndex);
assert.notStrictEqual(threeFolder, three);

console.error('test package.json require() loading');
assert.throws(
  function() {
    require('../fixtures/packages/invalid');
  },
  /^SyntaxError: Error parsing \S+: Unexpected token , in JSON at position 1$/
);

assert.strictEqual(require('../fixtures/packages/index').ok, 'ok',
                   'Failed loading package');
assert.strictEqual(require('../fixtures/packages/main').ok, 'ok',
                   'Failed loading package');
assert.strictEqual(require('../fixtures/packages/main-index').ok, 'ok',
                   'Failed loading package with index.js in main subdir');

console.error('test cycles containing a .. path');
const root = require('../fixtures/cycles/root');
const foo = require('../fixtures/cycles/folder/foo');
assert.strictEqual(root.foo, foo);
assert.strictEqual(root.sayHello(), root.hello);

console.error('test node_modules folders');
// asserts are in the fixtures files themselves,
// since they depend on the folder structure.
require('../fixtures/node_modules/foo');

console.error('test name clashes');
// this one exists and should import the local module
const my_path = require('../fixtures/path');
assert.ok(my_path.path_func instanceof Function);
// this one does not exist and should throw
assert.throws(function() { require('./utils'); },
              /^Error: Cannot find module '.\/utils'$/);

let errorThrown = false;
try {
  require('../fixtures/throws_error');
} catch (e) {
  errorThrown = true;
  assert.strictEqual('blah', e.message);
}

assert.strictEqual(require('path').dirname(__filename), __dirname);

console.error('load custom file types with extensions');
require.extensions['.test'] = function(module, filename) {
  let content = fs.readFileSync(filename).toString();
  assert.strictEqual('this is custom source\n', content);
  content = content.replace('this is custom source',
                            'exports.test = \'passed\'');
  module._compile(content, filename);
};

assert.strictEqual(require('../fixtures/registerExt').test, 'passed');
// unknown extension, load as .js
assert.strictEqual(require('../fixtures/registerExt.hello.world').test,
                   'passed');

console.error('load custom file types that return non-strings');
require.extensions['.test'] = function(module) {
  module.exports = {
    custom: 'passed'
  };
};

assert.strictEqual(require('../fixtures/registerExt2').custom, 'passed');

assert.strictEqual(require('../fixtures/foo').foo, 'ok',
                   'require module with no extension');

// Should not attempt to load a directory
try {
  require('../fixtures/empty');
} catch (err) {
  assert.strictEqual(err.message, 'Cannot find module \'../fixtures/empty\'');
}

// Check load order is as expected
console.error('load order');

const loadOrder = '../fixtures/module-load-order/';
const msg = 'Load order incorrect.';

require.extensions['.reg'] = require.extensions['.js'];
require.extensions['.reg2'] = require.extensions['.js'];

assert.strictEqual(require(loadOrder + 'file1').file1, 'file1', msg);
assert.strictEqual(require(loadOrder + 'file2').file2, 'file2.js', msg);
try {
  require(loadOrder + 'file3');
} catch (e) {
  // Not a real .node module, but we know we require'd the right thing.
  assert.ok(e.message.replace(/\\/g, '/').match(/file3\.node/));
}
assert.strictEqual(require(loadOrder + 'file4').file4, 'file4.reg', msg);
assert.strictEqual(require(loadOrder + 'file5').file5, 'file5.reg2', msg);
assert.strictEqual(require(loadOrder + 'file6').file6, 'file6/index.js', msg);
try {
  require(loadOrder + 'file7');
} catch (e) {
  assert.ok(e.message.replace(/\\/g, '/').match(/file7\/index\.node/));
}
assert.strictEqual(require(loadOrder + 'file8').file8, 'file8/index.reg', msg);
assert.strictEqual(require(loadOrder + 'file9').file9, 'file9/index.reg2', msg);


// make sure that module.require() is the same as
// doing require() inside of that module.
const parent = require('../fixtures/module-require/parent/');
const child = require('../fixtures/module-require/child/');
assert.strictEqual(child.loaded, parent.loaded);


// #1357 Loading JSON files with require()
const json = require('../fixtures/packages/main/package.json');
assert.deepStrictEqual(json, {
  name: 'package-name',
  version: '1.2.3',
  main: 'package-main-module'
});


// now verify that module.children contains all the different
// modules that we've required, and that all of them contain
// the appropriate children, and so on.

const children = module.children.reduce(function red(set, child) {
  let id = path.relative(path.dirname(__dirname), child.id);
  id = id.replace(/\\/g, '/');
  set[id] = child.children.reduce(red, {});
  return set;
}, {});

assert.deepStrictEqual(children, {
  'common.js': {},
  'fixtures/not-main-module.js': {},
  'fixtures/a.js': {
    'fixtures/b/c.js': {
      'fixtures/b/d.js': {},
      'fixtures/b/package/index.js': {}
    }
  },
  'fixtures/foo': {},
  'fixtures/nested-index/one/index.js': {
    'fixtures/nested-index/one/hello.js': {}
  },
  'fixtures/nested-index/two/index.js': {
    'fixtures/nested-index/two/hello.js': {}
  },
  'fixtures/nested-index/three.js': {},
  'fixtures/nested-index/three/index.js': {},
  'fixtures/packages/index/index.js': {},
  'fixtures/packages/main/package-main-module.js': {},
  'fixtures/packages/main-index/package-main-module/index.js': {},
  'fixtures/cycles/root.js': {
    'fixtures/cycles/folder/foo.js': {}
  },
  'fixtures/node_modules/foo.js': {
    'fixtures/node_modules/baz/index.js': {
      'fixtures/node_modules/bar.js': {},
      'fixtures/node_modules/baz/node_modules/asdf.js': {}
    }
  },
  'fixtures/path.js': {},
  'fixtures/throws_error.js': {},
  'fixtures/registerExt.test': {},
  'fixtures/registerExt.hello.world': {},
  'fixtures/registerExt2.test': {},
  'fixtures/empty.js': {},
  'fixtures/module-load-order/file1': {},
  'fixtures/module-load-order/file2.js': {},
  'fixtures/module-load-order/file3.node': {},
  'fixtures/module-load-order/file4.reg': {},
  'fixtures/module-load-order/file5.reg2': {},
  'fixtures/module-load-order/file6/index.js': {},
  'fixtures/module-load-order/file7/index.node': {},
  'fixtures/module-load-order/file8/index.reg': {},
  'fixtures/module-load-order/file9/index.reg2': {},
  'fixtures/module-require/parent/index.js': {
    'fixtures/module-require/child/index.js': {
      'fixtures/module-require/child/node_modules/target.js': {}
    }
  },
  'fixtures/packages/main/package.json': {}
});


// require() must take string, and must be truthy
assert.throws(function() {
  console.error('require non-string');
  require({ foo: 'bar' });
}, /path must be a string/);

assert.throws(function() {
  console.error('require empty string');
  require('');
}, /missing path/);

process.on('exit', function() {
  assert.ok(a.A instanceof Function);
  assert.strictEqual('A done', a.A());

  assert.ok(a.C instanceof Function);
  assert.strictEqual('C done', a.C());

  assert.ok(a.D instanceof Function);
  assert.strictEqual('D done', a.D());

  assert.ok(d.D instanceof Function);
  assert.strictEqual('D done', d.D());

  assert.ok(d2.D instanceof Function);
  assert.strictEqual('D done', d2.D());

  assert.strictEqual(true, errorThrown);

  console.log('exit');
});


// #1440 Loading files with a byte order marker.
assert.strictEqual(42, require('../fixtures/utf8-bom.js'));
assert.strictEqual(42, require('../fixtures/utf8-bom.json'));

// Error on the first line of a module should
// have the correct line number
assert.throws(function() {
  require('../fixtures/test-error-first-line-offset.js');
}, function(err) {
  return /test-error-first-line-offset.js:1:/.test(err.stack);
}, 'Expected appearance of proper offset in Error stack');
