// Tests resolving a path in the context of a spawned process.
// See https://github.com/nodejs/node/issues/7215
var path = require('path');
console.log(path.resolve(process.argv[2]));
