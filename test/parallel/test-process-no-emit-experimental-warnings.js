// Flags: --no-experimental-warnings --expose-internals
'use strict';
// Test supressing experimental warnings.
const common = require('../common');
const { hijackStderr,
        restoreStderr
} = require('../common/hijackstdio');
const { emitExperimentalWarning } = require('internal/util');

function test() {
  hijackStderr(common.mustNotCall('stderr.write must not be called'));
  emitExperimentalWarning('testExperimentalWarning');
  restoreStderr();
}

test();
