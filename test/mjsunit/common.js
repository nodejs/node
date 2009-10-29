exports.testDir = process.path.dirname(__filename);
exports.fixturesDir = process.path.join(exports.testDir, "fixtures");
exports.libDir = process.path.join(exports.testDir, "../../lib");

require.paths.unshift(exports.libDir);

var mjsunit = require("/mjsunit.js");
var utils = require("/utils.js");
process.mixin(exports, mjsunit, utils);
exports.posix = require("/posix.js");

