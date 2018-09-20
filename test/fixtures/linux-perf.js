'use strict';

const crypto = require('crypto');

// Functions should be complex enough for V8 to run them a few times before
// compiling, but not complex enough to always stay in interpreted mode. They
// should also take some time to run, otherwise Linux perf might miss them
// entirely even when sampling at a high frequency.
function functionOne(i) {
  for (let j=i; j > 0; j--) {
    crypto.createHash('md5').update(functionTwo(i, j)).digest("hex");
  }
}

function functionTwo(x, y) {
  let data = ((((x * y) + (x / y)) * y) ** (x + 1)).toString();
  if (x % 2 == 0) {
    return crypto.createHash('md5').update(data.repeat((x % 100) + 1)).digest("hex");
  } else {
    return crypto.createHash('md5').update(data.repeat((y % 100) + 1)).digest("hex");
  }
}

for (let i = 0; i < 1000; i++) {
  functionOne(i);
}
