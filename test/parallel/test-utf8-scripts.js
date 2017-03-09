'use strict';
require('../common');
const assert = require('assert');

// üäö

console.log('Σὲ γνωρίζω ἀπὸ τὴν κόψη');

assert.equal(true, /Hellö Wörld/.test('Hellö Wörld'));
