exports.testDir = node.path.dirname(__filename);
exports.fixturesDir = node.path.join(exports.testDir, "fixtures");
exports.libDir = node.path.join(exports.testDir, "../../lib");

node.libraryPaths.unshift(exports.libDir);

var mjsunit = require("/mjsunit.js");
var utils = require("/utils.js");
node.mixin(exports, mjsunit, utils);

