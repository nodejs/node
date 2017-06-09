'use strict';
const Path = require('path');

const { test } = require('tap');

const startCLI = require('./start-cli');

test('stepping through breakpoints', (t) => {
  const script = Path.join('examples', 'break.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:1`,
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
        `break in ${script}:2`,
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
        `break in ${script}:3`,
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
        `break in ${script}:10`,
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
      t.match(cli.output, `#0 ${script}:6`);
      t.match(cli.output, `#1 ${script}:16`);
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
        `break in ${script}:16`,
        'found breakpoint we set above w/ line number only');
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:6`,
        'found breakpoint we set above w/ line number & script');
    })
    .then(() => cli.stepCommand(''))
    .then(() => {
      t.match(
        cli.output,
        `debugCommand in ${script}:14`,
        'found function breakpoint we set above');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});

test('sb before loading file', (t) => {
  const script = Path.join('examples', 'cjs', 'index.js');
  const otherScript = Path.join('examples', 'cjs', 'other.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
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
        `break in ${otherScript}:3`,
        'found breakpoint in file that was not loaded yet');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});

test('clearBreakpoint', (t) => {
  const script = Path.join('examples', 'break.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('sb("break.js", 3)'))
    .then(() => cli.command('sb("break.js", 9)'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      t.match(cli.output, `#0 ${script}:3`);
      t.match(cli.output, `#1 ${script}:9`);
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
      t.match(cli.output, `#0 ${script}:9`);
    })
    .then(() => cli.stepCommand('cont'))
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:9`,
        'hits the 2nd breakpoint because the 1st was cleared');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
