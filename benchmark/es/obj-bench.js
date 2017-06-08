'use strict';

const common = require('../common.js');
const Obj1 = require('../fixtures/moduleA');
const Obj2 = require('../fixtures/moduleB');

const bench = common.createBenchmark(main, {
  // TODO(jasnell): A and B are not great labels, not sure what would be better
  method: ['A', 'B'],
  access: ['direct', 'indirect'],
  millions: [10]
});

function run(n, obj, direct) {
  var i;
  if (direct) {
    bench.start();
    for (i = 0; i < n; i++) {
      obj.A();
      obj.B();
      obj.C;
    }
    bench.end(n / 1e6);
  } else {
    const A = obj.A;
    const B = obj.B;
    bench.start();
    for (i = 0; i < n; i++) {
      A();
      B();
      obj.C;
    }
    bench.end(n / 1e6);
  }
}

function main(conf) {
  const n = +conf.millions * 1e6;
  const direct = conf.access === 'direct';

  switch (conf.method) {
    case 'A':
      // Uses the {A, B, C: 1}; Syntax
      run(n, Obj1, direct);
      break;
    case 'B':
      // Uses the obj = {}; obj.A = A; obj.B = B; obj.C = 1; Syntax
      run(n, Obj2, direct);
      break;
    default:
      throw new Error('Unexpected type');
  }
}
