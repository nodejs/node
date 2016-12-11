'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('run after quit / restart', (t) => {
  const cli = startCLI(['examples/three-lines.js']);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('breakpoints'))
    .then(() => {
      t.match(cli.output, 'No breakpoints yet');
    })
    .then(() => cli.command('sb(2)'))
    .then(() => cli.command('sb(3)'))
    .then(() => cli.command('breakpoints'))
    .then(() => {
      t.match(cli.output, '#0 examples/three-lines.js:2');
      t.match(cli.output, '#1 examples/three-lines.js:3');
    })
    .then(() => cli.stepCommand('c')) // hit line 2
    .then(() => cli.stepCommand('c')) // hit line 3
    .then(() => {
      t.match(cli.output, 'break in examples/three-lines.js:3');
    })
    .then(() => cli.command('restart'))
    .then(() => cli.waitFor([/break in examples/, /breakpoints restored/]))
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(cli.output, 'break in examples/three-lines.js:1');
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      t.match(cli.output, 'break in examples/three-lines.js:2');
    })
    .then(() => cli.stepCommand('c'))
    .then(() => {
      t.match(cli.output, 'break in examples/three-lines.js:3');
    })
    .then(() => cli.command('breakpoints'))
    .then(() => {
      t.match(cli.output, '#0 examples/three-lines.js:2');
      t.match(cli.output, '#1 examples/three-lines.js:3');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
