var assert = require('assert');

// https://github.com/nodejs/node/issues/1803
// this module is used as a preload module. It should have a parent with the
// module search paths initialized from the current working directory
assert.ok(module.parent);
var expectedPaths = require('module')._nodeModulePaths(process.cwd());
assert.deepEqual(module.parent.paths, expectedPaths);

var cluster = require('cluster');
cluster.isMaster || process.exit(42 + cluster.worker.id); // +42 to distinguish
// from exit(1) for other random reasons
