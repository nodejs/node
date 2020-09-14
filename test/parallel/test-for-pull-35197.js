//todo

'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}
