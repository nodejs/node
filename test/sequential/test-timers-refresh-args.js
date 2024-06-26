'use strict';
require('../common');
const assert = require('assert');

const LONGER_DELAY = 1000;
const SHORTER_DELAY = 100;
let last;
let t = setTimeout(() => {
  if (last !== undefined) {
    assert(Date.now() - last < LONGER_DELAY);

    last = undefined;
    let count = 0;
    t = setInterval(() => {
      if (last !== undefined)
        assert(Date.now() - last < LONGER_DELAY);
      last = Date.now();
      switch (count++) {
        case 0:
          t.refresh(SHORTER_DELAY, true);
          break;
        case 3:
          clearInterval(t);
          break;
      }
    }, LONGER_DELAY);
    return;
  }
  last = Date.now();
  t.refresh(SHORTER_DELAY);
}, LONGER_DELAY);
