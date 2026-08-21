// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-async-stack-tagging-api

const { session, contextGroup, Protocol } = InspectorTest.start('Stack tagging API works');

session.setupScriptMap();

// We write the test cases as functions instead of string literals so the
// fuzzer can fuzz them as well.

const testCases = [
  function simpleTaskLogsCorrectAsyncTrace() {
    function foo() { return console.createTask('Task'); }
    const task = foo();
    task.run(function runner() {
      console.trace('Inside run');
    });
  },
  function nestedTasksLogCorrectAsyncTrace() {
    const outerTask = console.createTask('Outer Task');
    outerTask.run(function runOuter() {
      const innerTask = console.createTask('Inner Task');
      innerTask.run(function runInner() {
        console.trace('Inside runInner');
      });
    });
  },
  async function setTimeoutWorksCorrectly() {
    const outerTask = console.createTask('Outer Task');
    await outerTask.run(async function runOuter() {
      return new Promise(r => setTimeout(() => {
        const innerTask = console.createTask('Inner Task');
        innerTask.run(function runInner() {
          console.trace('Inside runInner');
          r();
        });
      }, 0));
    });
  },
  function runForwardsTheReturnValue() {
    const task = console.createTask('Task');
    const result = task.run(() => 42);
    console.log(result);
  },
  async function runWorksWithAsyncPayloads() {
    const task = console.createTask('Task');
    const result = await task.run(async () => 42);
    console.log(result);
  },
  function runWorksWithGenerators() {
    const task = console.createTask('Task');
    const iter = task.run(function* () { yield 42; });
    console.log(iter.next().value);
  },
  function runCanBeCalledMultipleTimes() {
    const task = console.createTask('Task');
    task.run(() => console.log('First run'));
    task.run(() => console.log('Second run'));
  },
  function runForwardsExceptions() {
    const task = console.createTask('Task');
    task.run(() => {
      throw new Error('Thrown from task.run');
    });
  },
  function recursivelyCalledRunDoesntCrash() {
    const outerTask = console.createTask('Outer Task');
    outerTask.run(function runOuter() {
      const innerTask = console.createTask('Inner Task');
      innerTask.run(function runInner() {
        outerTask.run(function nestedRunOuter() {
          console.trace('Inside nestedRunOuter');
        });
      });
    });
  },
  function scheduleThrowsAnErrorWhenCalledWithoutAnArgument() {
    console.createTask();
  },
  function scheduleThrowsAnErrorWhenCalledWithAnEmptyString() {
    console.createTask('');
  },
  function runThrowsAnErrorWhenCalledWithoutAnArgument() {
    const task = console.createTask('Task');
    task.run();
  },
  function runThrowsAnErrorWhenCalledWithNonFunction() {
    const task = console.createTask('Task');
    task.run(42);
  },
  function runThrowsAnErrorWhenCalledWithNullReceiver() {
    const task = console.createTask('Task');
    task.run.call(undefined, () => { });
  },
  function runThrowsAnErrorWhenCalledWithIllegalReceiver() {
    const task = console.createTask('Task');
    task.run.call({}, () => { });  // The private symbol is missing.
  },
];

(async () => {
  Protocol.Runtime.onConsoleAPICalled(({params}) => {
    InspectorTest.log(`---------- console.${params.type}: ${params.args[0].value} ----------`);
    session.logCallFrames(params.stackTrace.callFrames);
    session.logAsyncStackTrace(params.stackTrace.parent);
  });
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });

  for (const testCase of testCases) {
    InspectorTest.log(`\nRunning test: ${testCase.name}`);

    // Turn the function into an IIFE and compile + run it as a script.
    const {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: `(${testCase.toString()})();`,
      sourceURL: `${testCase.name}.js`,
      persistScript: true,
    });
    const { result: { exceptionDetails } } = await Protocol.Runtime.runScript({
      scriptId,
      awaitPromise: true,
    });
    if (exceptionDetails) {
      InspectorTest.log(`Expected exception: `);
      InspectorTest.logMessage(exceptionDetails.exception);
    }
  }

  InspectorTest.completeTest();
})();
