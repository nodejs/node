'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');

const assertValues = [undefined, null, 0, true, false, 1];

assertValues.forEach((assertValue) => {
  assert.throws(
    () => fs.mkdtempSync(assertValue, {}),
    /^TypeError: filename prefix is required$/
  );
});
