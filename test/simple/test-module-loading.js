common = require("../common");
assert = common.assert
var path = require('path');

common.debug("load test-module-loading.js");

var a = require("../fixtures/a");
var c = require("../fixtures/b/c");
var d = require("../fixtures/b/d");
var d2 = require("../fixtures/b/d");
// Absolute
var d3 = require(path.join(__dirname, "../fixtures/b/d"));
// Relative
var d4 = require("../fixtures/b/d");

assert.equal(false, false, "testing the test program.");

assert.equal(true, a.A instanceof Function);
assert.equal("A", a.A());

assert.equal(true, a.C instanceof Function);
assert.equal("C", a.C());

assert.equal(true, a.D instanceof Function);
assert.equal("D", a.D());

assert.equal(true, d.D instanceof Function);
assert.equal("D", d.D());

assert.equal(true, d2.D instanceof Function);
assert.equal("D", d2.D());

assert.equal(true, d3.D instanceof Function);
assert.equal("D", d3.D());

assert.equal(true, d4.D instanceof Function);
assert.equal("D", d4.D());

assert.ok((new a.SomeClass) instanceof c.SomeClass);

common.debug("test index.js modules ids and relative loading")
var one = require("../fixtures/nested-index/one"),
  two = require("../fixtures/nested-index/two");
assert.notEqual(one.hello, two.hello);

common.debug("test cycles containing a .. path");
var root = require("../fixtures/cycles/root"),
  foo = require("../fixtures/cycles/folder/foo");
assert.equal(root.foo, foo);
assert.equal(root.sayHello(), root.hello);

common.debug("test name clashes");
// this one exists and should import the local module
var my_path = require("./path");
assert.equal(true, my_path.path_func instanceof Function);
// this one does not exist and should throw
assert.throws(function() { require("./utils")});

var errorThrown = false;
try {
  require("../fixtures/throws_error");
} catch (e) {
  errorThrown = true;
  assert.equal("blah", e.message);
}

var errorThrownAsync = false;
require.async("../fixtures/throws_error1", function(err, a) {
  if (err) {
    errorThrownAsync = true;
    assert.equal("blah", err.message);
  }
});

assert.equal(require('path').dirname(__filename), __dirname);

var asyncRun = false;
require.async('../fixtures/a1', function (err, a) {
  if (err) throw err;
  assert.equal("A", a.A());
  asyncRun = true;
});

common.debug('load custom file types with registerExtension');
require.registerExtension('.test', function(content) {
  assert.equal("this is custom source\n", content);

  return content.replace("this is custom source", "exports.test = 'passed'");
});

assert.equal(require('../fixtures/registerExt').test, "passed");

common.debug('load custom file types that return non-strings');
require.registerExtension('.test', function(content) {
  return {
    custom: 'passed'
  };
});

assert.equal(require('../fixtures/registerExt2').custom, 'passed');
debug("load modules by absolute id, then change require.paths, and load another module with the same absolute id.");
// this will throw if it fails.
var foo = require("../fixtures/require-path/p1/foo");
process.assert(foo.bar.expect === foo.bar.actual);

assert.equal(require('../fixtures/foo').foo, 'ok',
  'require module with no extension');

// Should not attempt to load a directory
try {
  require("../fixtures/empty");
} catch(err) {
  assert.equal(err.message, "Cannot find module '../fixtures/empty'");
}

var asyncRequireDir = false;
require.async("../fixtures/empty", function (err, a) {
  assert.ok(err);

  if (err) {
    asyncRequireDir = true;
    assert.equal(err.message, "Cannot find module '../fixtures/empty'");
  }
});

// Check load order is as expected
common.debug('load order');

var loadOrder = '../fixtures/module-load-order/',
    msg       = "Load order incorrect.";

require.registerExtension('.reg',  function(content) { return content; });
require.registerExtension('.reg2', function(content) { return content; });

assert.equal(require(loadOrder + 'file1').file1, 'file1',            msg);
assert.equal(require(loadOrder + 'file2').file2, 'file2.js',         msg);
try {
  require(loadOrder + 'file3');
} catch (e) {
  // Not a real .node module, but we know we require'd the right thing.
  assert.ok(e.message.match(/^dlopen/));
  assert.ok(e.message.match(/file3\.node/));
}
assert.equal(require(loadOrder + 'file4').file4, 'file4.reg',        msg);
assert.equal(require(loadOrder + 'file5').file5, 'file5.reg2',       msg);
assert.equal(require(loadOrder + 'file6').file6, 'file6/index.js',   msg);
try {
  require(loadOrder + 'file7');
} catch (e) {
  assert.ok(e.message.match(/^dlopen/));
  assert.ok(e.message.match(/file7\/index\.node/));
}
assert.equal(require(loadOrder + 'file8').file8, 'file8/index.reg',  msg);
assert.equal(require(loadOrder + 'file9').file9, 'file9/index.reg2', msg);

process.addListener("exit", function () {
  assert.equal(true, a.A instanceof Function);
  assert.equal("A done", a.A());

  assert.equal(true, a.C instanceof Function);
  assert.equal("C done", a.C());

  assert.equal(true, a.D instanceof Function);
  assert.equal("D done", a.D());

  assert.equal(true, d.D instanceof Function);
  assert.equal("D done", d.D());

  assert.equal(true, d2.D instanceof Function);
  assert.equal("D done", d2.D());

  assert.equal(true, errorThrown);

  assert.equal(true, asyncRun);

  assert.equal(true, errorThrownAsync);

  assert.equal(true, asyncRequireDir);

  console.log("exit");
});
