var assert = require('assert');
var util = require('util');

assert(
  util.deepEqual(
    { a : [ 2, 3 ], b : [ 4 ] },
    { a : [ 2, 3 ], b : [ 4 ] }
  ),
  'Objects must be deep equal'
);

assert.equal(
  false,
  util.deepEqual(
    { x : 5, y : [6] },
    { x : 5, y : 6 }
  ),
  '[6] and 6 should compare as not equal'
);

assert(
  util.deepEqual(
    { x : 5, y : '6' },
    { x : 5, y : 6 }
  ),
  '"6" and 6 should compare as equal'
);
