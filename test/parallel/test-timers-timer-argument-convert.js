'use strict';
const common = require('../common');

const cb = common.mustNotCall(() => { });

const timer1 = setTimeout(cb, 1000);
clearTimeout({
  valueOf() { return timer1; }
});

const timer2 = setTimeout(cb, 1000);
clearTimeout({
  toString() { return timer2; }
});

const timer3 = setTimeout(cb, 1000);
clearTimeout({
  [Symbol.toPrimitive](hint) {
    return timer3;
  }
});

const timer4 = setTimeout(cb, 1000);
clearTimeout({
  valueOf() { return timer4; },
  toString() { return {}; }
});
