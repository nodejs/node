'use strict';
const common = require('../common.js');
const { URLSearchParams } = require('url');

const inputs = {
  noencode: 'foo=bar&baz=quux&xyzzy=thud',
  // multicharsep: 'foo=bar&&&&&&&&&&baz=quux&&&&&&&&&&xyzzy=thud',
  multicharsep: '&&&&&&&&&&&&&&&&&&&&&&&&&&&&',
  encodemany: '%66%6F%6F=bar&%62%61%7A=quux&xyzzy=%74h%75d',
  encodelast: 'foo=bar&baz=quux&xyzzy=thu%64',
  multivalue: 'foo=bar&foo=baz&foo=quux&quuy=quuz',
  multivaluemany: 'foo=bar&foo=baz&foo=quux&quuy=quuz&foo=abc&foo=def&' +
                  'foo=ghi&foo=jkl&foo=mno&foo=pqr&foo=stu&foo=vwxyz',
  manypairs: 'a&b&c&d&e&f&g&h&i&j&k&l&m&n&o&p&q&r&s&t&u&v&w&x&y&z'
};

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [1e5]
});

function main(conf) {
  const input = inputs[conf.type];
  const n = conf.n | 0;

  var i;
  bench.start();
  for (i = 0; i < n; i++)
    new URLSearchParams(input);
  bench.end(n);
}
