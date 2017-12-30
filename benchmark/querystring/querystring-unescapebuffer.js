'use strict';
const common = require('../common.js');
const querystring = require('querystring');

const bench = common.createBenchmark(main, {
  input: [
    'there is nothing to unescape here',
    'there%20are%20several%20spaces%20that%20need%20to%20be%20unescaped',
    'there%2Qare%0-fake%escaped values in%%%%this%9Hstring',
    '%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%30%31%32%33%34%35%36%37'
  ],
  n: [10e6],
});

function main({ input, n }) {
  bench.start();
  for (var i = 0; i < n; i += 1)
    querystring.unescapeBuffer(input);
  bench.end(n);
}
