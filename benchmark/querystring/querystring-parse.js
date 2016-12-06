'use strict';
var common = require('../common.js');
var querystring = require('querystring');
var v8 = require('v8');

var types = [
  'noencode',
  'multicharsep',
  'encodemany',
  'encodelast',
  'multivalue',
  'multivaluemany',
  'manypairs'
];

var bench = common.createBenchmark(main, {
  type: types,
  n: [1e6],
});

function main(conf) {
  var type = conf.type;
  var n = conf.n | 0;

  var inputs = {
    noencode: 'foo=bar&baz=quux&xyzzy=thud',
    multicharsep: 'foo=bar&&&&&&&&&&baz=quux&&&&&&&&&&xyzzy=thud',
    encodemany: '%66%6F%6F=bar&%62%61%7A=quux&xyzzy=%74h%75d',
    encodelast: 'foo=bar&baz=quux&xyzzy=thu%64',
    multivalue: 'foo=bar&foo=baz&foo=quux&quuy=quuz',
    multivaluemany: 'foo=bar&foo=baz&foo=quux&quuy=quuz&foo=abc&foo=def&' +
                    'foo=ghi&foo=jkl&foo=mno&foo=pqr&foo=stu&foo=vwxyz',
    manypairs: 'a&b&c&d&e&f&g&h&i&j&k&l&m&n&o&p&q&r&s&t&u&v&w&x&y&z'
  };
  var input = inputs[type];

  // Force-optimize querystring.parse() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  v8.setFlagsFromString('--allow_natives_syntax');
  if (type !== 'multicharsep') {
    querystring.parse(input);
    eval('%OptimizeFunctionOnNextCall(querystring.parse)');
    querystring.parse(input);
  } else {
    querystring.parse(input, '&&&&&&&&&&');
    eval('%OptimizeFunctionOnNextCall(querystring.parse)');
    querystring.parse(input, '&&&&&&&&&&');
  }

  var i;
  if (type !== 'multicharsep') {
    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input);
    bench.end(n);
  } else {
    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');
    bench.end(n);
  }
}
