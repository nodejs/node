var path = require("path");
require("../common");

var f = __filename;

assert.equal(path.basename(f), "test-path.js");
assert.equal(path.basename(f, ".js"), "test-path");
assert.equal(path.extname(f), ".js");
assert.equal(path.dirname(f).substr(-11), "test/simple");
assert.equal(path.dirname("/a/b"), "/a");
assert.equal(path.dirname("/a"), "/");
assert.equal(path.dirname("/"), "/");
path.exists(f, function (y) { assert.equal(y, true) });

assert.equal(path.join(".", "fixtures/b", "..", "/b/c.js"), "fixtures/b/c.js");

assert.equal(path.normalize("./fixtures///b/../b/c.js"), "fixtures/b/c.js");
assert.equal(path.normalize("./fixtures///b/../b/c.js",true), "fixtures///b/c.js");

assert.deepEqual(path.normalizeArray(["fixtures","b","","..","b","c.js"]), ["fixtures","b","c.js"]);
assert.deepEqual(path.normalizeArray(["fixtures","","b","..","b","c.js"], true), ["fixtures","","b","c.js"]);

assert.equal(path.normalize("a//b//../b", true), "a//b/b");
assert.equal(path.normalize("a//b//../b"), "a/b");

assert.equal(path.normalize("a//b//./c", true), "a//b//c");
assert.equal(path.normalize("a//b//./c"), "a/b/c");
assert.equal(path.normalize("a//b//.", true), "a//b/");
assert.equal(path.normalize("a//b//."), "a/b");

