'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');

(async () => {
  await assert.rejects(
    fs.promises.statfs(),
    (err) => {
      assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
      return true;
    }
  );
})().then(common.mustCall());
