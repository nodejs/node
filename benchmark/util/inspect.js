'use strict';
var util = require('util');

var common = require('../common.js');

var bench = common.createBenchmark(main, {n: [3e6]});

function main(conf) {
  var n = conf.n | 0;

  bench.start();
  for (var i = 0; i < n; i += 1) {
    util.inspect({
      a: undefined,
      b: null,
      c: [, // test for empty array index
        -0,
        0,
        '1',
        Buffer.from('2'),
        Symbol('3')
      ],
      d: new Set([
        function() {},
        function fn() {},
        () => {}
      ]),
      e: new WeakSet([
        new Promise(() => {}),
        async () => {}
      ]),
      f: new Map([
        [/regexp/, new Date()],
      ]),
      g: new WeakMap([
        [new Int8Array(), new Uint8Array()],
        [new Int16Array(), new Uint16Array()],
        [new Int32Array(), new Uint32Array()]
      ])
    });
  }
  bench.end(n);
}
