'use strict';

// Compares the non-throwing header validators (http.isValidHeaderName() and
// http.isValidHeaderValue()) with the throwing ones (http.validateHeaderName()
// and http.validateHeaderValue()) used inside try/catch.

const common = require('../common.js');
const assert = require('assert');
const {
  isValidHeaderName,
  isValidHeaderValue,
  validateHeaderName,
  validateHeaderValue,
} = require('http');

const inputs = {
  name: {
    valid: [
      'ETag', 'date', 'Vary', 'server', 'Expires', 'location', 'Connection',
      'content-type', 'Cache-Control', 'content-length', 'x-frame-options',
      'Transfer-Encoding', 'x-request-id',
    ],
    invalid: [
      '', ':', 'bad header', 'x-forwarded-fםr', '中文呢', '((((())))',
      ':alternate-protocol', 'alternate-protocol:', 'x\r\ninjected',
    ],
  },
  value: {
    valid: [
      'W/"2-d4cbb29"', 'OK', 'Express', 'application/json',
      'application/json; charset=utf-8', 'sessionid=; Path=/',
      'text/html; charset=utf-8', '10', 'max-age=0, no-cache', 'gzip, br',
    ],
    // Invalid under both 'strict' and 'relaxed' validation.
    invalid: [
      'a\r\nb', 'value\n', 'cr\r', 'nul\0byte', 'לא תקין', 'emoji \u{1F600}',
      'x'.repeat(64) + '\r\n',
    ],
  },
};

const bench = common.createBenchmark(main, {
  method: [
    'isValidHeaderName',
    'validateHeaderName',
    'isValidHeaderValue',
    'validateHeaderValue',
  ],
  input: ['valid', 'invalid'],
  httpValidation: ['strict', 'relaxed'],
  n: [1e6],
}, {
  // httpValidation only applies to isValidHeaderValue().
  combinationFilter: (p) =>
    p.httpValidation === 'strict' || p.method === 'isValidHeaderValue',
});

function main({ n, method, input, httpValidation }) {
  let valid = 0;

  switch (method) {
    case 'isValidHeaderName': {
      const list = inputs.name[input];
      const len = list.length;
      bench.start();
      for (let i = 0; i < n; i++) {
        if (isValidHeaderName(list[i % len])) valid++;
      }
      bench.end(n);
      break;
    }
    case 'validateHeaderName': {
      const list = inputs.name[input];
      const len = list.length;
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          validateHeaderName(list[i % len]);
          valid++;
        } catch {
          // Invalid name.
        }
      }
      bench.end(n);
      break;
    }
    case 'isValidHeaderValue': {
      const list = inputs.value[input];
      const len = list.length;
      const options = httpValidation === 'strict' ? undefined : { httpValidation };
      bench.start();
      for (let i = 0; i < n; i++) {
        if (isValidHeaderValue(list[i % len], options)) valid++;
      }
      bench.end(n);
      break;
    }
    case 'validateHeaderValue': {
      const list = inputs.value[input];
      const len = list.length;
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          validateHeaderValue('x-header', list[i % len]);
          valid++;
        } catch {
          // Invalid value.
        }
      }
      bench.end(n);
      break;
    }
    default:
      throw new Error(`Unexpected method: ${method}`);
  }

  // Consume the result so the loop cannot be optimized away, and make sure
  // the inputs are what they claim to be.
  assert.strictEqual(valid, input === 'valid' ? n : 0);
}
