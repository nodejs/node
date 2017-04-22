'use strict';
const common = require('../common');
const assert = require('assert');

const N = 3;
let count = 0;
function next() {
  const immediate = setImmediate(common.mustCall(() => {
    clearImmediate(immediate);
    ++count;
  }));
}

for(let i = 0; i < N; i++) {
  next();
}