'use strict';
const Path = require('path');

const { test } = require('tap');

const startCLI = require('./start-cli');

test('list scripts', (t) => {
  const script = Path.join('examples', 'empty.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => cli.command('scripts'))
    .then(() => {
      t.match(
        cli.output,
        /^\* \d+: examples(?:\/|\\)empty\.js/,
        'lists the user script');
      t.notMatch(
        cli.output,
        /\d+: module\.js <native>/,
        'omits node-internal scripts');
    })
    .then(() => cli.command('scripts(true)'))
    .then(() => {
      t.match(
        cli.output,
        /\* \d+: examples(?:\/|\\)empty\.js/,
        'lists the user script');
      t.match(
        cli.output,
        /\d+: module\.js <native>/,
        'includes node-internal scripts');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
