var assert = require('assert');
var util = require('util');

assert(
  util.deepStrictEqual(
    { a : [ 2, 3 ], b : [ 4 ] },
    { a : [ 2, 3 ], b : [ 4 ] }
  ),
  'Objects must be strictly deep equal'
);

assert.equal(
  false,
  util.deepStrictEqual(
    { x : 5, y : '6' },
    { x : 5, y : 6 }
  ),
  '"6" and 6 should compare as not strictly equal'
);

assert.equal(
  false,
  util.deepStrictEqual(
    { x : 5, y : [6] },
    { x : 5, y : 6 }
  ),
  '[6] and 6 should compare as not strictly equal'
);
