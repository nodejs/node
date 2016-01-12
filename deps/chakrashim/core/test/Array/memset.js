//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Compares the value set by interpreter with the jitted code
// need to run with -mic:1 -off:simplejit -off:JITLoopBody -off:inline
// Run locally with -trace:memop -trace:bailout to help find bugs

let testCases = [
  function() {
    return {
      start: 0,
      end: 100,
      test: function testBasic(a) {
        for(let i = 0; i < 100; i++) {
          a[i] = 0;
        }
      }
    };
  },
  function() {
    return {
      start: 5,
      end: 101,
      test: function testReverse(a) {
        for(let i = 100; i >= 5; i--) {
          a[i] = 0;
        }
      }
    };
  },
  function() {
    let results = [];
    return {
      runner: function testMultipleMemset(arrayGen) {
        let a = arrayGen(), b = arrayGen(), c = arrayGen();
        for(let i = 0; i < 10; i++) {
          a[i] = b[i] = c[i] = 0;
        }
        results.push([a, b, c]);
      },
      check: function() {
        let base = results[0];
        for(let i = 1; i < results.length; ++i) {
          for(let j = 0; j < 3; ++j) {
            compareResult(base[j], results[i][j], 0, 10);
          }
        }
      }
    };
  },
  function() {
    return {
      start: 4,
      end: 30,
      test: function testUnroll(a) {
        for(let i = 4; i < 30;) {
          a[i] = 0;
          i++;
          a[i] = 0;
          i++;
        }
      }
    };
  },
  function() {
    return {
      start: 8,
      end: 10,
      test: function testMissingValues(a) {
        for(let i = 8; i < 10; i++) {
          a[i] = 0;
        }
      }
    };
  },
  function() {
    return {
      start: 0,
      end: 6,
      test: function testOverwrite(a) {
        a[5] = 3;
        for(let i = 0; i < 6; i++) {
          a[i] = 0;
        }
      }
    };
  },
  function() {
    return {
      start: 10,
      end: 50,
      test: function testNegativeConstant(a) {
        for(let i = 10; i < 50; i++) {
          a[i] = -1;
        }
      }
    };
  },
  function() {
    return {
      start: -50,
      end: 10,
      test: function testNegativeStartIndex(a) {
        for(let i = -50; i < 10; i++) {
          a[i] = -3;
        }
      }
    };
  }
];

let arrayGenerators = [
  // the one for the interpreter
  function() {
    return new Array(10);
  },
  function() {
    return new Array(10);
  },
  function() {
    return [];
  }
  // causes bailouts right now: BailOut: function: testMultipleMemset ( (#1.2), #3) offset: #0036 Opcode: BailOnNotArray Kind: BailOutOnNotNativeArray
  // function() {return [1, 2, 3, 4, 5, 6, 7]; }
];

for(let testCase of testCases) {
  let results = [];
  let testInfo = testCase();
  for(let gen of arrayGenerators) {
    if(testInfo.runner) {
      let result = testInfo.runner(gen);
      results.push(result);
    } else {
      let newArray = gen();
      testInfo.test(newArray);
      results.push(newArray);
    }
  }

  if(testInfo.check) {
    testInfo.check(results);
  } else {
    let base = results[0]; // result from the interpreter
    for(let i = 1; i < results.length; ++i) {
      compareResult(base, results[i], testInfo.start, testInfo.end);
    }
  }
}
let passed = true;
function compareResult(a, b, start, end) {
  for(let i = start; i < end; i++) {
    if(a[i] !== b[i]) {
      print(`${i}: ${a[i]} !== ${b[i]}`);
      passed = false;
      return false;
    }
  }
  return true;
}

if(passed) {
  print("PASSED");
} else {
  print("FAILED");
}

