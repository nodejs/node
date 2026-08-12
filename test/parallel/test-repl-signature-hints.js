'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

(async () => {
  {
    // Test 1: Typing `foo(` shows a signature hint with parameters.
    const { replServer, input, output, run } = startNewREPLServer({
      useColors: false,
    });

    await run(['function foo(a, b, c) { return a + b + c; }']);
    output.accumulator = '';

    input.emit('data', 'foo(');
    await delay(500);

    assert.ok(
      output.accumulator.includes('foo(a, b, c)'),
      `Expected signature hint "foo(a, b, c)", got:\n${output.accumulator}`
    );
    replServer.close();
  }

  {
    // Test 2: Zero-param function should not display a hint.
    const { replServer, input, output, run } = startNewREPLServer({
      useColors: false,
    });

    await run(['function bar() { return 42; }']);
    output.accumulator = '';

    input.emit('data', 'bar(');
    await delay(500);

    assert.ok(
      !output.accumulator.includes('// bar()'),
      `Did not expect a hint for zero-param function, got:\n${output.accumulator}`
    );
    replServer.close();
  }

  {
    // Test 3: Dotted access — `console.log(` should show a hint.
    const { replServer, input, output } = startNewREPLServer({
      useColors: false,
    });

    output.accumulator = '';
    input.emit('data', 'console.log(');
    await delay(500);

    assert.ok(
      output.accumulator.includes('console.log('),
      `Expected signature hint for console.log, got:\n${output.accumulator}`
    );
    replServer.close();
  }

  {
    // Test 4: Non-function should NOT show a hint.
    const { replServer, input, output, run } = startNewREPLServer({
      useColors: false,
    });

    await run(['const x = 42']);
    output.accumulator = '';

    input.emit('data', 'x(');
    await delay(500);

    assert.ok(
      !output.accumulator.includes('// x('),
      `Did not expect a hint for a non-function, got:\n${output.accumulator}`
    );
    replServer.close();
  }

  {
    // Test 5: With useColors, hint should use gray ANSI styling.
    const { replServer, input, output, run } = startNewREPLServer({
      useColors: true,
    });

    await run(['function greet(name) { return name; }']);
    output.accumulator = '';

    input.emit('data', 'greet(');
    await delay(500);

    const gray = '\x1b[90m';
    assert.ok(
      output.accumulator.includes(gray) &&
      output.accumulator.includes('greet(name)'),
      `Expected gray-styled hint for greet(name), got:\n${output.accumulator}`
    );
    replServer.close();
  }

  {
    // Test 6: preview=false should not show signature hints.
    const { replServer, input, output, run } = startNewREPLServer({
      useColors: false,
      preview: false,
    });

    await run(['function noHint(z) { return z; }']);
    output.accumulator = '';

    input.emit('data', 'noHint(');
    await delay(500);

    assert.ok(
      !output.accumulator.includes('noHint(z)'),
      `Did not expect a hint with preview=false, got:\n${output.accumulator}`
    );
    replServer.close();
  }

  {
    // Test 7: Arrow functions should show hints.
    const { replServer, input, output, run } = startNewREPLServer({
      useColors: false,
    });

    await run(['const add = (x, y) => x + y']);
    output.accumulator = '';

    input.emit('data', 'add(');
    await delay(500);

    assert.ok(
      output.accumulator.includes('add(x, y)'),
      `Expected signature hint for arrow function, got:\n${output.accumulator}`
    );
    replServer.close();
  }

  {
    // Test 8: Hint is cleared on next keypress.
    const { replServer, input, output, run } = startNewREPLServer({
      useColors: false,
    });

    await run(['function clear(a) { return a; }']);
    output.accumulator = '';

    input.emit('data', 'clear(');
    await delay(500);
    assert.ok(
      output.accumulator.includes('clear(a)'),
      `Expected hint to appear, got:\n${output.accumulator}`
    );

    // Typing another character should clear the hint.
    output.accumulator = '';
    input.emit('data', '1');
    await delay(200);

    // The clear operation writes ANSI escape sequences to erase the hint line.
    // After clearing, the hint text should not be re-rendered.
    assert.ok(
      !output.accumulator.includes('// clear(a)'),
      `Expected hint to be cleared after typing, got:\n${output.accumulator}`
    );
    replServer.close();
  }
})().then(common.mustCall());
