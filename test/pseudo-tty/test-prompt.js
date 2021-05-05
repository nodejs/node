'use strict';

const common = require('../common');
const assert = require('assert');

(async () => {
  const resp1 = prompt('what your favorite food?');
  assert.strictEqual(resp1, 'sushi');

  const resp2 = prompt('what is the default value?', 'DEFAULT');
  assert.strictEqual(resp2, 'DEFAULT');
})().then(common.mustCall(() => {}));
