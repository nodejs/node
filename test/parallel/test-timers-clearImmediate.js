'use strict';
const common = require('../common');

// run clearImmediate in the callback.
for (let i = 0; i < 3; ++i) {
  const immediate = setImmediate(common.mustCall(function() {
    clearImmediate(immediate);
  }));
}

// run clearImmediate before the callback.
for (let i = 0; i < 3; ++i) {
  const immediate = setImmediate(common.mustNotCall(function() {}));

  clearImmediate(immediate);
}
