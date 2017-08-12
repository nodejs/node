// Flags: --stack_trace_limit=3

'use strict';
require('../common');

async function f() {
  await f();
}

async function g() {
  try {
    await f();
  } catch (e) {
    console.log(e);
  }
}

g();
