'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

const command = `.load ${fixtures.path('repl-load-multiline.js')}`;
const terminalCode = '\u001b[1G\u001b[0J \u001b[1G';
const terminalCodeRegex = new RegExp(terminalCode.replace(/\[/g, '\\['), 'g');

const expected = `${command}
const getLunch = () =>
  placeOrder('tacos')
    .then(eat);

const placeOrder = (order) => Promise.resolve(order);
const eat = (food) => '<nom nom nom>';

undefined
`;

const { replServer, output } = startNewREPLServer();

replServer.write(`${command}\n`);
assert.strictEqual(output.accumulator.replace(terminalCodeRegex, ''), expected);
replServer.close();
