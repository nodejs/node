'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('stepping through breakpoints', (t) => {
  const cli = startCLI(['examples/break.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(
        cli.output,
        'break in examples/break.js:1',
        'pauses in the first line of the script');
      t.match(
        cli.output,
        /> 1 \(function \([^)]+\) \{ const x = 10;/,
        'shows the source and marks the current line');
    })
    .then(() => cli.stepCommand('n'))
    .then(() => {
      t.match(
        cli.output,
        'break in examples/break.js:2',
        'pauses in next line of the script');
      t.match(
        cli.output,
        '> 2 let name = \'World\';',
        'marks the 2nd line');
    })
    .then(() => cli.stepCommand('next'))
    .then(() => {
      t.match(
        cli.output,
        'break in examples/break.js:3',
        'pauses in next line of the script');
      t.match(
        cli.output,
        '> 3 name = \'Robin\';',
        'marks the 3nd line');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      t.match(
        cli.output,
        'break in examples/break.js:10',
        'pauses on the next breakpoint');
      t.match(
        cli.output,
        '>10 debugger;',
        'marks the debugger line');
    })

    // Prepare additional breakpoints
    .then(() => cli.command('sb("break.js", 6)'))
    .then(() => t.notMatch(cli.output, 'Could not resolve breakpoint'))
    .then(() => cli.command('sb("otherFunction()")'))
    .then(() => cli.command('sb(16)'))
    .then(() => t.notMatch(cli.output, 'Could not resolve breakpoint'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      t.match(cli.output, '#0 examples/break.js:6');
      t.match(cli.output, '#1 examples/break.js:16');
    })

    .then(() => cli.command('list()'))
    .then(() => {
      t.match(cli.output, '>10 debugger;', 'prints and marks current line');
      t.strictDeepEqual(
        cli.parseSourceLines(),
        [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
        'prints 5 lines before and after');
    })
    .then(() => cli.command('list(2)'))
    .then(() => {
      t.match(cli.output, '>10 debugger;', 'prints and marks current line');
      t.strictDeepEqual(
        cli.parseSourceLines(),
        [8, 9, 10, 11, 12],
        'prints 2 lines before and after');
    })

    .then(() => cli.stepCommand('s'))
    .then(() => cli.stepCommand(''))
    .then(() => {
      t.match(
        cli.output,
        'break in timers.js',
        'entered timers.js');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      t.match(
        cli.output,
        'break in examples/break.js:16',
        'found breakpoint we set above w/ line number only');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      t.match(
        cli.output,
        'break in examples/break.js:6',
        'found breakpoint we set above w/ line number & script');
    })
    .then(() => cli.stepCommand(''))
    .then(() => {
      t.match(
        cli.output,
        'debugCommand in examples/break.js:14',
        'found function breakpoint we set above');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});

test('sb before loading file', (t) => {
  const cli = startCLI(['examples/cjs/index.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('sb("other.js", 3)'))
    .then(() => {
      t.match(
        cli.output,
        'not loaded yet',
        'warns that the script was not loaded yet');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      t.match(
        cli.output,
        'break in examples/cjs/other.js:3',
        'found breakpoint in file that was not loaded yet');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});

test('clearBreakpoint', (t) => {
  const cli = startCLI(['examples/break.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('sb("break.js", 3)'))
    .then(() => cli.command('sb("break.js", 9)'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      t.match(cli.output, '#0 examples/break.js:3');
      t.match(cli.output, '#1 examples/break.js:9');
    })
    .then(() => cli.command('clearBreakpoint("break.js", 4)'))
    .then(() => {
      t.match(cli.output, 'Could not find breakpoint');
    })
    .then(() => cli.command('clearBreakpoint("not-such-script.js", 3)'))
    .then(() => {
      t.match(cli.output, 'Could not find breakpoint');
    })
    .then(() => cli.command('clearBreakpoint("break.js", 3)'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      t.match(cli.output, '#0 examples/break.js:9');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      t.match(
        cli.output,
        'break in examples/break.js:9',
        'hits the 2nd breakpoint because the 1st was cleared');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
