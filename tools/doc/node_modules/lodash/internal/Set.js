var getNative = require('./getNative');

/* Built-in method references that are verified to be native. */
var Set = getNative(global, 'Set');

module.exports = Set;
