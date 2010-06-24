require("../common");
var path = require('path');

debug("load test-module-loading.js");

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

debug("test index.js modules ids and relative loading")
var one = require("../fixtures/nested-index/one"),
  two = require("../fixtures/nested-index/two");
assert.notEqual(one.hello, two.hello);

debug("test cycles containing a .. path");
var root = require("../fixtures/cycles/root"),
  foo = require("../fixtures/cycles/folder/foo");
assert.equal(root.foo, foo);
assert.equal(root.sayHello(), root.hello);

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

debug('load custom file types with registerExtension');
require.registerExtension('.test', function(content) {
  assert.equal("this is custom source\n", content);

  return content.replace("this is custom source", "exports.test = 'passed'");
});

assert.equal(require('../fixtures/registerExt').test, "passed");

debug('load custom file types that return non-strings');
require.registerExtension('.test', function(content) {
  return {
    custom: 'passed'
  };
});

assert.equal(require('../fixtures/registerExt2').custom, 'passed');

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

  console.log("exit");
});
