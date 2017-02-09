'use strict';
var common = require('../common.js');
var querystring = require('querystring');
var v8 = require('v8');
var inputs = require('../fixtures/url-inputs.js').searchParams;

var bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [1e6],
});

// A deopt followed by a reopt of main() can happen right when the timed loop
// starts, which seems to have a noticeable effect on the benchmark results.
// So we explicitly disable optimization of main() to avoid this potential
// issue.
v8.setFlagsFromString('--allow_natives_syntax');
eval('%NeverOptimizeFunction(main)');

function main(conf) {
  var type = conf.type;
  var n = conf.n | 0;
  var input = inputs[type];
  var i;

  // Note: we do *not* use OptimizeFunctionOnNextCall() here because currently
  // it causes a deopt followed by a reopt, which could make its way into the
  // timed loop. Instead, just execute the function a "sufficient" number of
  // times before the timed loop to ensure the function is optimized just once.
  if (type === 'multicharsep') {
    for (i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');

    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');
    bench.end(n);
  } else {
    for (i = 0; i < n; i += 1)
      querystring.parse(input);

    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input);
    bench.end(n);
  }
}
