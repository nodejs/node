import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

const command = `.load ${fixtures.path('repl-load-multiline.js')}`;
// eslint-disable-next-line no-control-regex
const terminalCodeRegex = /\x1b\[1G\x1b\[0J(?: |\| )\x1b\[[13]G/g;

const expected = `${command}
const getLunch = () =>
  placeOrder('tacos')
    .then(eat);

const placeOrder = (order) => Promise.resolve(order);
const eat = (food) => '<nom nom nom>';

undefined
`;

const { replServer, output, run } = startNewREPLServer();

await run(`${command}\n`);
assert.strictEqual(output.accumulator.replace(terminalCodeRegex, ''), expected);
replServer.close();
