require.paths.unshift(__dirname);
exports.bar = require('bar'); // surprise! this is not /p2/bar, this is /p1/bar
