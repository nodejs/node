'use strict';

require('../common');
const assert = require('assert');
const repl = require('repl');
const ArrayStream = require('../common/arraystream');

// \u001b[1G - Moves the cursor to 1st column
// \u001b[0J - Clear screen
// \u001b[3G - Moves the cursor to 3rd column
const terminalCode = '\u001b[1G\u001b[0J> \u001b[3G';
const terminalCodeRegex = new RegExp(terminalCode.replace(/\[/g, '\\['), 'g');

function run({ input, output, event, checkTerminalCodes = true }) {
  const stream = new ArrayStream();
  let found = '';

  stream.write = (msg) => found += msg.replace('\r', '');

  let expected = `${terminalCode}.editor\n` +
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

  if (!checkTerminalCodes) {
    found = found.replace(terminalCodeRegex, '').replace(/\n/g, '');
    expected = expected.replace(terminalCodeRegex, '').replace(/\n/g, '');
  }

  assert.strictEqual(found, expected);
}

const tests = [
  {
    input: '',
    output: '\n(To exit, press ^C again or type .exit)',
    event: { ctrl: true, name: 'c' }
  },
  {
    input: 'var i = 1;',
    output: '',
    event: { ctrl: true, name: 'c' }
  },
  {
    input: 'var i = 1;\ni + 3',
    output: '\n4',
    event: { ctrl: true, name: 'd' }
  },
  {
    input: '  var i = 1;\ni + 3',
    output: '\n4',
    event: { ctrl: true, name: 'd' }
  },
  {
    input: '',
    output: '',
    checkTerminalCodes: false,
    event: null,
  }
];

tests.forEach(run);

// Auto code alignment for .editor mode
function testCodeAlignment({ input, cursor = 0, line = '' }) {
  const stream = new ArrayStream();
  const outputStream = new ArrayStream();

  stream.write = () => { throw new Error('Writing not allowed!'); };

  const replServer = repl.start({
    prompt: '> ',
    terminal: true,
    input: stream,
    output: outputStream,
    useColors: false
  });

  stream.emit('data', '.editor\n');
  input.split('').forEach((ch) => stream.emit('data', ch));
  // Test the content of current line and the cursor position
  assert.strictEqual(line, replServer.line);
  assert.strictEqual(cursor, replServer.cursor);

  replServer.write('', { ctrl: true, name: 'd' });
  replServer.close();
  // Ensure that empty lines are not saved in history
  assert.notStrictEqual(replServer.history[0].trim(), '');
}

const codeAlignmentTests = [
  {
    input: 'var i = 1;\n'
  },
  {
    input: '  var i = 1;\n',
    cursor: 2,
    line: '  '
  },
  {
    input: '     var i = 1;\n',
    cursor: 5,
    line: '     '
  },
  {
    input: ' var i = 1;\n var j = 2\n',
    cursor: 2,
    line: '  '
  }
];

codeAlignmentTests.forEach(testCodeAlignment);
