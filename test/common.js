var path = require("path");

exports.testDir = path.dirname(__filename);
exports.fixturesDir = path.join(exports.testDir, "fixtures");
exports.libDir = path.join(exports.testDir, "../lib");
exports.PORT = 12346;

exports.assert = require('assert');

var sys = require("sys");
for (var i in sys) exports[i] = sys[i];
for (var i in exports) global[i] = exports[i];
