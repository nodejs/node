require("../common");

path = require("path");

isDebug = (process.version.indexOf('debug') >= 0);

nodePath = path.join(__dirname,
                     "..",
                     "..",
                     "build",
                     isDebug ? 'debug' : 'default',
                     isDebug ? 'node_g' : 'node');
nodePath = path.normalize(nodePath);

console.log('nodePath: ' + nodePath);
console.log('process.execPath: ' + process.execPath);


assert.equal(nodePath, process.execPath);
