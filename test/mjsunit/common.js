var path = require("path");

exports.testDir = path.dirname(__filename);
exports.fixturesDir = path.join(exports.testDir, "fixtures");
exports.libDir = path.join(exports.testDir, "../../lib");

require.paths.unshift(exports.libDir);

var mjsunit = require("mjsunit");
var sys = require("sys");

process.mixin(exports, mjsunit, sys);
exports.posix = require("posix");
exports.path = path;

