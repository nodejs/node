'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');

(async () => {
  await assert.rejects(
    fs.promises.statfs(),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
})().then(common.mustCall());
