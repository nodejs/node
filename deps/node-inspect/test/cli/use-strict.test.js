'use strict';
const Path = require('path');

const { test } = require('tap');

const startCLI = require('./start-cli');

test('for whiles that starts with strict directive', (t) => {
  const script = Path.join('examples', 'use-strict.js');
  const cli = startCLI([script]);

  function onFatal(error) {
    cli.quit();
    throw error;
  }

  return cli.waitFor(/break/)
    .then(() => cli.waitForPrompt())
    .then(() => {
      t.match(
        cli.output,
        /break in [^:]+:(?:1|2)[^\d]/,
        'pauses either on strict directive or first "real" line');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
