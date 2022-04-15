'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');

// Check if the two constants accepted by chmod() on Windows are defined.
if (common.isWindows) {
  assert(fs.constants.S_IRUSR !== undefined)
  assert(fs.constants.S_IWUSR !== undefined)
}
