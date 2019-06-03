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

  return cli.waitForInitialBreak()
    .then(() => cli.waitForPrompt())
    .then(() => {
      const brk = cli.breakInfo;
      t.match(
        `${brk.line}`,
        /^(1|2)$/,
        'pauses either on strict directive or first "real" line');
    })
    .then(() => cli.quit())
    .then(null, onFatal);
});
