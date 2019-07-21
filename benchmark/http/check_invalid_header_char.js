'use strict';

const common = require('../common.js');
const _checkInvalidHeaderChar = require('_http_common')._checkInvalidHeaderChar;

const groupedInputs = {
  // Representative set of inputs from an AcmeAir benchmark run:
  // all valid strings, average length 14.4, stdev 13.0
  group_acmeair: [
    'W/"2-d4cbb29"', 'OK', 'Express', 'X-HTTP-Method-Override', 'Express',
    'application/json', 'application/json; charset=utf-8', '206', 'OK',
    'sessionid=; Path=/', 'text/html; charset=utf-8',
    'text/html; charset=utf-8', '10', 'W/"a-eda64de5"', 'OK', 'Express',
    'application/json', 'application/json; charset=utf-8', '2', 'W/"2-d4cbb29"',
    'OK', 'Express', 'X-HTTP-Method-Override', 'sessionid=; Path=/', 'Express',
    'sessionid=; Path=/,sessionid=6b059402-d62f-4e6f-b3dd-ce5b9e487c39; Path=/',
    'text/html; charset=utf-8', 'text/html; charset=utf-8', '9', 'OK',
    'sessionid=; Path=/', 'text/html; charset=utf-8',
    'text/html; charset=utf-8', '10', 'W/"a-eda64de5"', 'OK', 'Express',
    'Express', 'X-HTTP-Method-Override', 'sessionid=; Path=/',
    'application/json',
  ],

  // Put it here so the benchmark result lines will not be super long.
  LONG_AND_INVALID: ['Here is a value that is really a folded header ' +
    'value\r\n  this should be supported, but it is not currently']
};

const inputs = [
  // Valid
  '',
  '1',
  '\t\t\t\t\t\t\t\t\t\tFoo bar baz',
  'keep-alive',
  'close',
  'gzip',
  '20091',
  'private',
  'text/html; charset=utf-8',
  'text/plain',
  'Sat, 07 May 2016 16:54:48 GMT',
  'SAMEORIGIN',
  'en-US',

  // Invalid
  '中文呢', // unicode
  'foo\nbar',
  '\x7F',
];

const bench = common.createBenchmark(main, {
  input: inputs.concat(Object.keys(groupedInputs)),
  n: [1e6],
});

function main({ n, input }) {
  let inputs = [input];
  if (groupedInputs.hasOwnProperty(input)) {
    inputs = groupedInputs[input];
  }

  const len = inputs.length;
  bench.start();
  for (let i = 0; i < n; i++) {
    _checkInvalidHeaderChar(inputs[i % len]);
  }
  bench.end(n);
}
