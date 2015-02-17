var assert = require('assert');

var initial_dir  = process.cwd();
var expected_dir = initial_dir + '/test'

/**
 * Test Asynchronus
 */
process.chdir("test", function(err) {
    if (err) throw err;

    assert.equal(expected_dir, process.cwd(), "expected to be in a different directory");
});
assert.equal(initial_dir, process.cwd(), "expected to be in the initial directory");


/**
 * Test Synchronous
 */
process.chdir("test");
assert.equal(expected_dir, process.cwd(), "expected to be in a different directory");

/* Tear Down */
process.chdir("..");
assert.equal(initial_dir, process.cwd(), "expected to be in the initial directory");
