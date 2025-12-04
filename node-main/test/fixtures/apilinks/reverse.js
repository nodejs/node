'use strict';

// Parallel assignment to the exported variable and module.exports.

function ok() {
}

const asserts = module.exports = ok;

asserts.ok = ok;

asserts.strictEqual = function() {
}
