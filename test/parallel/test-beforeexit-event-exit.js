'use strict';
const common = require('../common');

process.on('beforeExit', function() {
  common.fail('exit should not allow this to occur');
});

process.exit();
