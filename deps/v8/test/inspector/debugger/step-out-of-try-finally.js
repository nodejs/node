// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping out of functions that return through a finally block.');

const source = `
function inner() { return 1; }

function returnInTry() {
  try {
    debugger;
    return inner();
  } finally {
    globalThis.seen = 1;
  }
}

function debuggerInFinally() {
  try {
    return inner();
  } finally {
    debugger;
  }
}

function returnFromForOf() {
  for (const x of [1, 2]) {
    debugger;
    return x;
  }
}

function returnFromUsing() {
  using r = {[Symbol.dispose]() {}};
  debugger;
  return 1;
}

function nestedTryFinally() {
  try {
    try {
      debugger;
      return inner();
    } finally {
      globalThis.seen = 1;
    }
  } finally {
    globalThis.seen = 2;
  }
}

function twoReturns(x) {
  try {
    debugger;
    if (x) return 1;
    return 2;
  } finally {
    globalThis.seen = 1;
  }
}

function fallThrough(x) {
  try {
    if (x) return 1;
  } finally {
    globalThis.seen = 1;
  }
}  // fallThrough end

async function asyncReturnInTry() {
  try {
    debugger;
    return 1;
  } finally {
    globalThis.seen = 1;
  }
}

function* gen() {
  try {
    yield 1;
  } finally {
    globalThis.seen = 1;
  }
}  // gen end

function* genWithReturn() {
  try {
    yield 1;
    return 2;
  } finally {
    debugger;
  }
}

async function* asyncGen() {
  try {
    debugger;
    return 1;
  } finally {
    globalThis.seen = 1;
  }
}

// The body of every async generator is wrapped in an implicit finally block.
async function* plainAsyncGen() {
  debugger;
  return 1;
}

async function* asyncGenFallOff() {
  debugger;
  globalThis.seen = 1;
}

async function* asyncGenWithYield() {
  yield 1;
  globalThis.seen = 1;
}  // asyncGenWithYield end

class Base {}
class Derived extends Base {
  constructor() {
    super();
    try {
      debugger;
      return;
    } finally {
      globalThis.seen = 1;
    }
  }
}

class PlainDerived extends Base {
  constructor() {
    super();
    debugger;
    return;
  }
}

function call(f, ...args) {
  const result = f(...args);
  return result;
}

function construct(C) {
  const result = new C();
  return result;
}

async function awaitNext(g) {
  const result = await g.next();
  return result;
}

function drain(g) {
  const values = [...g];
  return values;
}

function returnEarly(g) {
  g.next();
  const result = g.return(42);
  return result;
}

async function drainAsync(g) {
  const values = [];
  for await (const value of g) values.push(value);
  return values;
}

async function returnEarlyAsync(g) {
  await g.next();
  const result = await g.return(42);
  return result;
}
//# sourceURL=test.js`;

const lines = source.split('\n');
function lineOf(marker) {
  return lines.findIndex(line => line.includes(marker));
}

session.setupScriptMap();
contextGroup.addScript(source);

// Evaluates `expression` and, starting from its first pause, performs
// `actions` one after another, logging every pause. A step that is lost lets
// the evaluation run to completion, which is logged instead of a location.
async function runSteps(expression, actions) {
  let paused = Protocol.Debugger.oncePaused();
  const done = Protocol.Runtime.evaluate({expression, awaitPromise: true})
                   .then(() => null);
  let message = await Promise.race([paused, done]);
  for (const action of actions) {
    if (!message) break;
    await session.logSourceLocation(message.params.callFrames[0].location);
    InspectorTest.log(`Debugger.${action}`);
    paused = Protocol.Debugger.oncePaused();
    Protocol.Debugger[action]();
    message = await Promise.race([paused, done]);
  }
  if (message) {
    await session.logSourceLocation(message.params.callFrames[0].location);
    Protocol.Debugger.resume();
    await done;
  } else {
    InspectorTest.log('Evaluation finished without pausing.');
    // A lost step stays pending; reset so it cannot leak into the next test.
    await Protocol.Debugger.disable();
    await Protocol.Debugger.enable();
  }
}

async function withBreakpointAt(marker, body) {
  const {result: {breakpointId, locations}} =
      await Protocol.Debugger.setBreakpointByUrl(
          {url: 'test.js', lineNumber: lineOf(marker), columnNumber: 0});
  InspectorTest.log(`Breakpoint set at:`);
  await session.logSourceLocation(locations[0]);
  await body();
  await Protocol.Debugger.removeBreakpoint({breakpointId});
}

let scriptId;

InspectorTest.runAsyncTestSuite([
  async function enable() {
    Protocol.Debugger.onScriptParsed(({params}) => {
      if (params.url === 'test.js') scriptId = params.scriptId;
    });
    await Protocol.Debugger.enable();
  },

  async function breakLocations() {
    const {result: {locations}} =
        await Protocol.Debugger.getPossibleBreakpoints(
            {start: {lineNumber: 0, columnNumber: 0, scriptId}});
    await session.logBreakLocations(locations);
  },

  async function stepOutOfReturnInTry() {
    await runSteps('call(returnInTry)', ['stepOut']);
  },

  async function stepOverOffTheEndOfFinally() {
    await runSteps(
        'call(returnInTry)', ['stepOver', 'stepOver', 'stepOver', 'stepOver']);
  },

  async function stepIntoOffTheEndOfFinally() {
    await runSteps(
        'call(returnInTry)', ['stepOver', 'stepOver', 'stepInto', 'stepInto']);
  },

  async function stepOutOfFinally() {
    await runSteps('call(debuggerInFinally)', ['stepOut']);
  },

  async function stepOutOfForOf() {
    await runSteps('call(returnFromForOf)', ['stepOut']);
  },

  async function stepOutOfUsing() {
    await runSteps('call(returnFromUsing)', ['stepOut']);
  },

  async function stepOutOfNestedTryFinally() {
    await runSteps('call(nestedTryFinally)', ['stepOut']);
  },

  async function stepOutOfFirstOfTwoReturns() {
    await runSteps('call(twoReturns, true)', ['stepOut']);
  },

  async function stepOutOfSecondOfTwoReturns() {
    await runSteps('call(twoReturns, false)', ['stepOut']);
  },

  async function stepOverFirstOfTwoReturns() {
    // Both returns share the Return emitted after the finally block, which has
    // the position of the last one.
    await runSteps(
        'call(twoReturns, true)',
        ['stepOver', 'stepOver', 'stepOver', 'stepOver', 'stepOver']);
  },

  async function stepOutOfAsyncFunction() {
    await runSteps('call(asyncReturnInTry)', ['stepOut']);
  },

  async function stepOutOfAsyncGenerator() {
    await runSteps('awaitNext(asyncGen())', ['stepOut']);
  },

  async function stepOutOfPlainAsyncGenerator() {
    await runSteps('awaitNext(plainAsyncGen())', ['stepOut']);
  },

  async function stepOverOffTheEndOfPlainAsyncGenerator() {
    await runSteps(
        'awaitNext(plainAsyncGen())', ['stepOver', 'stepOver', 'stepOver']);
  },

  async function stepOverOffTheEndOfAsyncGeneratorFallingOff() {
    await runSteps(
        'awaitNext(asyncGenFallOff())', ['stepOver', 'stepOver', 'stepOver']);
  },

  async function stepOutOfDerivedConstructor() {
    await runSteps('construct(Derived)', ['stepOut']);
  },

  async function stepOverOffTheEndOfFinallyInDerivedConstructor() {
    await runSteps(
        'construct(Derived)',
        ['stepOver', 'stepOver', 'stepOver', 'stepOver', 'stepOver']);
  },

  async function stepOverReturnInPlainDerivedConstructor() {
    await runSteps(
        'construct(PlainDerived)', ['stepOver', 'stepOver', 'stepOver']);
  },

  async function stepOutOfFinallyDuringGeneratorReturn() {
    await runSteps('returnEarly(genWithReturn())', ['stepOut']);
  },

  async function closingBraceBreakpointOnFallThrough() {
    await withBreakpointAt(
        '// fallThrough end', () => runSteps('call(fallThrough, false)', []));
  },

  async function closingBraceBreakpointOnReturnInTry() {
    await withBreakpointAt(
        '// fallThrough end', () => runSteps('call(fallThrough, true)', []));
  },

  async function generatorClosingBraceBreakpointOnCompletion() {
    await withBreakpointAt('// gen end', () => runSteps('drain(gen())', []));
  },

  async function generatorClosingBraceBreakpointOnReturn() {
    await withBreakpointAt(
        '// gen end', () => runSteps('returnEarly(gen())', []));
  },

  async function asyncGeneratorClosingBraceBreakpointOnCompletion() {
    await withBreakpointAt(
        '// asyncGenWithYield end',
        () => runSteps('drainAsync(asyncGenWithYield())', []));
  },

  async function asyncGeneratorClosingBraceBreakpointOnReturn() {
    await withBreakpointAt(
        '// asyncGenWithYield end',
        () => runSteps('returnEarlyAsync(asyncGenWithYield())', []));
  },
]);
