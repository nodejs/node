'use strict';
const Path = require('path');

const { test } = require('tap');

const startCLI = require('./start-cli');

test('examples/empty.js', (t) => {
  const script = Path.join('examples', 'empty.js');
  const cli = startCLI([script]);

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(cli.output, 'debug>', 'prints a prompt');
      t.match(
        cli.output,
        '< Debugger listening on port 9229',
        'forwards child output');
    })
    .then(() => cli.command('["hello", "world"].join(" ")'))
    .then(() => {
      t.match(cli.output, 'hello world', 'prints the result');
    })
    .then(() => cli.command(''))
    .then(() => {
      t.match(cli.output, 'hello world', 'repeats the last command on <enter>');
    })
    .then(() => cli.command('version'))
    .then(() => {
      t.match(cli.output, process.versions.v8, 'version prints the v8 version');
    })
    .then(() => cli.quit())
    .then((code) => {
      t.equal(code, 0, 'exits with success');
    });
});

test('run after quit / restart', (t) => {
  const script = Path.join('examples', 'three-lines.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.stepCommand('n'))
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:2`,
        'steps to the 2nd line');
    })
    .then(() => cli.command('cont'))
    .then(() => cli.waitFor(/disconnect/))
    .then(() => {
      t.match(
        cli.output,
        'Waiting for the debugger to disconnect',
        'the child was done');
    })
    .then(() => {
      // On windows the socket won't close by itself
      return cli.command('kill');
    })
    .then(() => cli.command('cont'))
    .then(() => cli.waitFor(/start the app/))
    .then(() => {
      t.match(cli.output, 'Use `run` to start the app again');
    })
    .then(() => cli.stepCommand('run'))
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:1`,
        'is back at the beginning');
    })
    .then(() => cli.stepCommand('n'))
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:2`,
        'steps to the 2nd line');
    })
    .then(() => cli.stepCommand('restart'))
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:1`,
        'is back at the beginning');
    })
    .then(() => cli.command('kill'))
    .then(() => cli.command('cont'))
    .then(() => cli.waitFor(/start the app/))
    .then(() => {
      t.match(cli.output, 'Use `run` to start the app again');
    })
    .then(() => cli.stepCommand('run'))
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(
        cli.output,
        `break in ${script}:1`,
        'is back at the beginning');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
