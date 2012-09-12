var commondir = require('commondir');
var dir = commondir('/foo/bar', [ '../baz', '../../foo/quux', './bizzy' ])
console.log(dir);
