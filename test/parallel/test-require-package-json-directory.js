'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

common.expectsError(
  () => { require(fixtures.path('module-package-json-directory')); },
  {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module/
  });
