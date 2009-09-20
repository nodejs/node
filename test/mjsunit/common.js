exports.testDir = node.path.dirname(__filename);
exports.fixturesDir = node.path.join(exports.testDir, "fixtures");
exports.libDir = node.path.join(exports.testDir, "../../lib");

node.libraryPaths.unshift(exports.libDir);

var mjsunit = require("/mjsunit.js");
// Copy mjsunit namespace out
for (var prop in mjsunit) {
  if (mjsunit.hasOwnProperty(prop)) exports[prop] = mjsunit[prop];
}


