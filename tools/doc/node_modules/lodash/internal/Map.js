var getNative = require('./getNative');

/* Built-in method references that are verified to be native. */
var Map = getNative(global, 'Map');

module.exports = Map;
