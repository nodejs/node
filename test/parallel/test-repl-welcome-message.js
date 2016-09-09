'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

// \u001b[1G - Moves the cursor to 1st column
// \u001b[0J - Clear screen
// \u001b[3G - Moves the cursor to 3rd column
const terminalCode = '\u001b[1G\u001b[0J> \u001b[3G';
const version = process.version;
const jsEngine = process.jsEngine || 'v8';
const jsEngineVersion = process.versions[jsEngine];
const docURL = `https://nodejs.org/dist/${version}/docs/api/repl.html`;
const welcomeMessage =
  `Welcome to Node.js ${version} (${jsEngine} VM,` +
  ` ${jsEngineVersion})\nType ^M or enter to execute,` +
  ' ^J to continue, ^C to exit\n' +
  `Or try .help for help, more at ${docURL}\n\n`;

function run(displayWelcomeMessage, callback) {
  const putIn = new common.ArrayStream();
  let output = '';
  putIn.write = (data) => (output += data);
  const replServer = repl.start({
    prompt: '> ',
    input: putIn,
    output: putIn,
    terminal: true,
    displayWelcomeMessage
  });
  replServer.close();
  callback(output);
}

// when no welcome message
run(false, common.mustCall((output) => {
  assert.strictEqual(output, terminalCode);
}));

// show welcome message
run(true, common.mustCall((output) => {
  assert.strictEqual(output, `${welcomeMessage}${terminalCode}`);
}));
