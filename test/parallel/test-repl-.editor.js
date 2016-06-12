'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

// \u001b[1G - Moves the cursor to 1st column
// \u001b[0J - Clear screen
// \u001b[3G - Moves the cursor to 3rd column
const terminalCode = '\u001b[1G\u001b[0J> \u001b[3G';

function run(input, output, event) {
  const stream = new common.ArrayStream();
  let found = '';

  stream.write = (msg) => found += msg.replace('\r', '');

  const expected = `${terminalCode}.editor\n` +
                   '// Entering editor mode (^D to finish, ^C to cancel)\n' +
                   `${input}${output}\n${terminalCode}`;

  const replServer = repl.start({
    prompt: '> ',
    terminal: true,
    input: stream,
    output: stream,
    useColors: false
  });

  stream.emit('data', '.editor\n');
  stream.emit('data', input);
  replServer.write('', event);
  replServer.close();
  assert.strictEqual(found, expected);
}

const tests = [
  {
    input: '',
    output: '\n(To exit, press ^C again or type .exit)',
    event: {ctrl: true, name: 'c'}
  },
  {
    input: 'var i = 1;',
    output: '',
    event: {ctrl: true, name: 'c'}
  },
  {
    input: 'var i = 1;\ni + 3',
    output: '\n4',
    event: {ctrl: true, name: 'd'}
  }
];

tests.forEach(({input, output, event}) => run(input, output, event));
