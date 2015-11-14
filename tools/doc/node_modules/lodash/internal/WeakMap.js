var getNative = require('./getNative');

/* Built-in method references that are verified to be native. */
var WeakMap = getNative(global, 'WeakMap');

module.exports = WeakMap;
