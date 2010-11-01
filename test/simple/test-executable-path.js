common = require("../common");
assert = common.assert;
path = require("path");

isDebug = (process.version.indexOf('debug') >= 0);

debugPath = path.normalize(path.join(__dirname, '..', '..', 'build', 'debug', 'node_g'));
defaultPath = path.normalize(path.join(__dirname, '..', '..', 'build', 'default', 'node'));

console.log('debugPath: ' + debugPath);
console.log('defaultPath: ' + defaultPath);
console.log('process.execPath: ' + process.execPath);

if (/node_g$/.test(process.execPath)) {
  assert.equal(debugPath, process.execPath);
} else {
  assert.equal(defaultPath, process.execPath);
}

