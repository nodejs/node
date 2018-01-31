'use strict';
const common = require('../common');

// Test that deprecation warning for custom inspection via the `.inspect()`
// property (on the target object) is emitted once and only once.

const util = require('util');

{
  const target = { inspect: () => 'Fhqwhgads' };
  // `common.expectWarning` will expect the warning exactly one time only
  common.expectWarning(
    'DeprecationWarning',
    'Custom inspection function on Objects via .inspect() is deprecated'
  );
  util.inspect(target);  // should emit deprecation warning
  util.inspect(target);  // should not emit deprecation warning
}
