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

var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

common.debug('load test-module-loading.js');

// assert that this is the main module.
assert.equal(require.main.id, '.', 'main module should have id of \'.\'');
assert.equal(require.main, module, 'require.main should === module');
assert.equal(process.mainModule, module,
             'process.mainModule should === module');
// assert that it's *not* the main module in the required module.
require('../fixtures/not-main-module.js');

// require a file with a request that includes the extension
var a_js = require('../fixtures/a.js');
assert.equal(42, a_js.number);

// require a file without any extensions
var foo_no_ext = require('../fixtures/foo');
assert.equal('ok', foo_no_ext.foo);

var a = require('../fixtures/a');
var c = require('../fixtures/b/c');
var d = require('../fixtures/b/d');
var d2 = require('../fixtures/b/d');
// Absolute
var d3 = require(path.join(__dirname, '../fixtures/b/d'));
// Relative
var d4 = require('../fixtures/b/d');

assert.equal(false, false, 'testing the test program.');

assert.equal(true, common.indirectInstanceOf(a.A, Function));
assert.equal('A', a.A());

assert.equal(true, common.indirectInstanceOf(a.C, Function));
assert.equal('C', a.C());

assert.equal(true, common.indirectInstanceOf(a.D, Function));
assert.equal('D', a.D());

assert.equal(true, common.indirectInstanceOf(d.D, Function));
assert.equal('D', d.D());

assert.equal(true, common.indirectInstanceOf(d2.D, Function));
assert.equal('D', d2.D());

assert.equal(true, common.indirectInstanceOf(d3.D, Function));
assert.equal('D', d3.D());

assert.equal(true, common.indirectInstanceOf(d4.D, Function));
assert.equal('D', d4.D());

assert.ok((new a.SomeClass) instanceof c.SomeClass);

common.debug('test index.js modules ids and relative loading');
var one = require('../fixtures/nested-index/one'),
    two = require('../fixtures/nested-index/two');
assert.notEqual(one.hello, two.hello);

common.debug('test index.js in a folder with a trailing slash');
var three = require('../fixtures/nested-index/three'),
    threeFolder = require('../fixtures/nested-index/three/'),
    threeIndex = require('../fixtures/nested-index/three/index.js');
assert.equal(threeFolder, threeIndex);
assert.notEqual(threeFolder, three);

common.debug('test package.json require() loading');
assert.equal(require('../fixtures/packages/main').ok, 'ok',
             'Failed loading package');
assert.equal(require('../fixtures/packages/main-index').ok, 'ok',
             'Failed loading package with index.js in main subdir');

common.debug('test cycles containing a .. path');
var root = require('../fixtures/cycles/root'),
    foo = require('../fixtures/cycles/folder/foo');
assert.equal(root.foo, foo);
assert.equal(root.sayHello(), root.hello);

common.debug('test node_modules folders');
// asserts are in the fixtures files themselves,
// since they depend on the folder structure.
require('../fixtures/node_modules/foo');

common.debug('test name clashes');
// this one exists and should import the local module
var my_path = require('./path');
assert.ok(common.indirectInstanceOf(my_path.path_func, Function));
// this one does not exist and should throw
assert.throws(function() { require('./utils')});

var errorThrown = false;
try {
  require('../fixtures/throws_error');
} catch (e) {
  errorThrown = true;
  assert.equal('blah', e.message);
}

assert.equal(require('path').dirname(__filename), __dirname);

common.debug('load custom file types with extensions');
require.extensions['.test'] = function(module, filename) {
  var content = fs.readFileSync(filename).toString();
  assert.equal('this is custom source\n', content);
  content = content.replace('this is custom source',
                            'exports.test = \'passed\'');
  module._compile(content, filename);
};

assert.equal(require('../fixtures/registerExt').test, 'passed');
// unknown extension, load as .js
assert.equal(require('../fixtures/registerExt.hello.world').test, 'passed');

common.debug('load custom file types that return non-strings');
require.extensions['.test'] = function(module, filename) {
  module.exports = {
    custom: 'passed'
  };
};

assert.equal(require('../fixtures/registerExt2').custom, 'passed');
common.debug('load modules by absolute id, then change require.paths, ' +
             'and load another module with the same absolute id.');
// this will throw if it fails.
var foo = require('../fixtures/require-path/p1/foo');
assert.ok(foo.bar.expect === foo.bar.actual);

assert.equal(require('../fixtures/foo').foo, 'ok',
             'require module with no extension');

// Should not attempt to load a directory
try {
  require('../fixtures/empty');
} catch (err) {
  assert.equal(err.message, 'Cannot find module \'../fixtures/empty\'');
}

// Check load order is as expected
common.debug('load order');

var loadOrder = '../fixtures/module-load-order/',
    msg = 'Load order incorrect.';

require.extensions['.reg'] = require.extensions['.js'];
require.extensions['.reg2'] = require.extensions['.js'];

assert.equal(require(loadOrder + 'file1').file1, 'file1', msg);
assert.equal(require(loadOrder + 'file2').file2, 'file2.js', msg);
try {
  require(loadOrder + 'file3');
} catch (e) {
  // Not a real .node module, but we know we require'd the right thing.
  assert.ok(e.message.match(/file3\.node/));
}
assert.equal(require(loadOrder + 'file4').file4, 'file4.reg', msg);
assert.equal(require(loadOrder + 'file5').file5, 'file5.reg2', msg);
assert.equal(require(loadOrder + 'file6').file6, 'file6/index.js', msg);
try {
  require(loadOrder + 'file7');
} catch (e) {
  assert.ok(e.message.match(/file7\/index\.node/));
}
assert.equal(require(loadOrder + 'file8').file8, 'file8/index.reg', msg);
assert.equal(require(loadOrder + 'file9').file9, 'file9/index.reg2', msg);

process.addListener('exit', function() {
  assert.ok(common.indirectInstanceOf(a.A, Function));
  assert.equal('A done', a.A());

  assert.ok(common.indirectInstanceOf(a.C, Function));
  assert.equal('C done', a.C());

  assert.ok(common.indirectInstanceOf(a.D, Function));
  assert.equal('D done', a.D());

  assert.ok(common.indirectInstanceOf(d.D, Function));
  assert.equal('D done', d.D());

  assert.ok(common.indirectInstanceOf(d2.D, Function));
  assert.equal('D done', d2.D());

  assert.equal(true, errorThrown);

  console.log('exit');
});
