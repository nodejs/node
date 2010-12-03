var path = require('path');

require.paths.unshift(path.join(__dirname, '../p2'));

exports.foo = require('foo');

exports.expect = require(path.join(__dirname, '../p2/bar'));
exports.actual = exports.foo.bar;
