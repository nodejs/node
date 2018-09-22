'use strict';

require('../common');
const assert = require('assert');


// TODO add a test that throws something (example wrong type of args)

// trivial case
assert.deepStrictEqual(
  assert.stringMatching([], []),
  true
);

// case with only string
assert.deepStrictEqual(
  assert.stringMatching('blublu', ['blublu', 'blu']),
  false
);

// case with regex
assert.deepStrictEqual(
  assert.stringMatching('blublu', ['blublu', /blu\.*/]),
  true
);

// case with regex and string
assert.deepStrictEqual(
  assert.stringMatching(['blublu', 'lol'], ['blublu', /blu\.*/]),
  false
);
