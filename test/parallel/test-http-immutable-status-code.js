'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

assert(http.STATUS_CODES);

for (const code in http.STATUS_CODES) {
  const before = http.STATUS_CODES[code];
  common.expectsError(
    () => { http.STATUS_CODES[code] = 'foo'; },
    {
      type: TypeError,
      message: `Cannot assign to read only property '${code}' ` +
               'of object \'#<Object>\''
    }
  );
  assert.strictEqual(before, http.STATUS_CODES[code]);
}
