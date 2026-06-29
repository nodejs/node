// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// running on old version takes approximately 50 seconds, so 50,000 milliseconds
new BenchmarkSuite('Big-Switch', [50000], [
  new Benchmark('Big-Switch', false, false, 0, BigSwitch),
]);

function BigSwitch() {
  "use strict";
  const n = 100000;
  const c = (a, b) => Array(a).fill().map((a, c) => b(c));

  Function('n, c',
    `
      const a = c(n, a => a);
      let ctr = 0;

      for(let i = 0; i !== (1+n); i++){
        switch(i){
          ${c(n, a => `case ${a}: ctr += i; break;`).join('\n')}
          default: ctr += i; break;
        }
      }

      return ctr;
  `)(n,c);
}
