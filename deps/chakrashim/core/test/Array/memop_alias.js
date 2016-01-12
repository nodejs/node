//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Compares the value set by interpreter with the jitted code
// need to run with -mic:1 -off:simplejit -off:jitloopbody
// Run locally with -trace:memop -trace:bailout to help find bugs

let size = 200;
let testError = Symbol();
let src = new Array(size);
for(let i = 0; i < size; ++i) {
  src[i] = i;
}

function crashBug1(a, b) {
  // This should obivously not trigger a memset
  for(let i = 0; i < size; ++i) {
    let c = [b[i]];
    c[i] = 9;
  }
  try {
    for(let i = 0; i < size; ++i) {
      let c = b[i] && undefined;
      c[i] = 9;
    }
    return testError;
  } catch(e) {

    // If everything goes well the above should throw
  }
}

function test2(a) {
  let c = new Array(size);
  for(let i = 0; i < size; ++i) {
    a[i] = src[i];
    c[i] = a[i];
  }
}

function invalidMemset(a, b) {
  // if a and b are the same, a[i]=0 will get hoisted and the result will be incorrect
  for(let i = 0; i < size; ++i) {
    b[i] = i;
    a[i] = 0;
  }
}

function memopOrder(a, b) {
  // In case of aliasing, these 2 memset must remains in this order
  // Check the order of memopOrder
  for(let i = 0; i < size; ++i) {
    b[i] = 4;
    a[i] = 5;
  }
}

function memsetUnroll(a, b) {
  // Check the order of memopOrder
  for(let i = 0; i < size; ++i) {
    b[i] = 4;
    a[i] = 5;
    ++i;
    a[i] = 5;
    b[i] = 4;
  }
}

function invalidMemcopy(a, b) {
  for(let i = 0; i < size; ++i) {
    b[i] = i;
    a[i] = b[i];
  }
}

function copySetMix1(a, b) {
  for(let i = 0; i < size; ++i) {
    b[i] = 5;
    a[i] = b[i];
  }
}

function copySetMix2(a, b) {
  for(let i = 0; i < size; ++i) {
    a[i] = b[i];
    b[i] = 5;
  }
}

function copySetMix3(a, b) {
  for(let i = 0; i < size; ++i) {
    b[i] = a[i];
    b[i] = 5;
  }
}

function copySetMix4(a, b) {
  for(let i = 0; i < size; ++i) {
    a[i] = 5;
    a[i] = b[i];
  }
}

function test(fn) {
  let name = fn.name;
  let interpretArray = new Array(size);
  interpretArray.fill(1);
  fn(interpretArray, interpretArray);
  let jitArray = new Array(size);
  jitArray.fill(1);
  let r = fn(jitArray, jitArray);
  if(r === testError) {
    print(`Error: ${name} had an internal error`);
    return false;
  }
  return compare(interpretArray, jitArray, name);
}

function compare(a, b, name) {
  for(let i = 0; i < size; ++i) {
    if(a[i] !== b[i]) {
      print(`Error: ${name} interpret[${i}] (${a[i]}) !== jit[${i}] (${b[i]})`);
      return false;
    }
  }
  return true;
}

let tests = [
  crashBug1, // fix then reactivate
  test2,
  memsetUnroll,
  memopOrder,
  invalidMemset,
  invalidMemcopy,
  copySetMix1,
  copySetMix2,
  copySetMix3,
  copySetMix4
];

let passed = true;
for(let testFn of tests) {
  passed &= test(testFn);
}

if(passed) {
  print("PASSED");
} else {
  print("FAILED");
}
