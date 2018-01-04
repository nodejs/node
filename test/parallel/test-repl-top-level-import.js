'use strict';

const common = require('../common');
const assert = require('assert');
const { stripVTControlCharacters } = require('internal/readline');
const repl = require('repl');

common.crashOnUnhandledRejection();

// Flags: --expose-internals --experimental-modules

const PROMPT = 'import repl > ';

class REPLStream extends common.ArrayStream {
  constructor() {
    super();
    this.waitingForResponse = false;
    this.lines = [''];
  }

  write(chunk, encoding, callback) {
    if (Buffer.isBuffer(chunk)) {
      chunk = chunk.toString(encoding);
    }
    const chunkLines = stripVTControlCharacters(chunk).split('\n');
    this.lines[this.lines.length - 1] += chunkLines[0];
    if (chunkLines.length > 1) {
      this.lines.push(...chunkLines.slice(1));
    }
    this.emit('line');
    if (callback) callback();
    return true;
  }

  wait(lookFor = PROMPT) {
    if (this.waitingForResponse) {
      throw new Error('Currently waiting for response to another command');
    }
    this.lines = [''];
    return common.fires(new Promise((resolve, reject) => {
      const onError = (err) => {
        this.removeListener('line', onLine);
        reject(err);
      };
      const onLine = () => {
        if (this.lines[this.lines.length - 1].includes(lookFor)) {
          this.removeListener('error', onError);
          this.removeListener('line', onLine);
          resolve(this.lines);
        }
      };
      this.once('error', onError);
      this.on('line', onLine);
    }), new Error(), 1000);
  }
}

const putIn = new REPLStream();
const testMe = repl.start({
  prompt: PROMPT,
  stream: putIn,
  terminal: true,
  useColors: false,
  breakEvalOnSigint: true
});

function runAndWait(cmds, lookFor) {
  const promise = putIn.wait(lookFor);
  for (const cmd of cmds) {
    if (typeof cmd === 'string') {
      putIn.run([cmd]);
    } else {
      testMe.write('', cmd);
    }
  }
  return promise;
}

async function runEach(cmds) {
  const out = [];
  for (const cmd of cmds) {
    const ret = await runAndWait([cmd]);
    out.push(...ret);
  }
  return out;
}

const file = './test/fixtures/esm-with-basic-exports.mjs';
async function main() {
  assert.deepStrictEqual(await runEach([
    `/* comment */ import { six } from "${file}"; six + 1;`
  ]), [
    `/* comment */ import { six } from "${file}"; six + 1;\r`,
    '7',
    PROMPT
  ]);

  assert.deepStrictEqual(await runEach([
    `import def from "${file}"`,
    'def',
  ]), [
    `import def from "${file}"\r`,
    'undefined',
    PROMPT,
    'def\r',
    '\'default\'',
    PROMPT
  ]);

  testMe.resetContext();

  assert.deepStrictEqual(await runEach([
    `import * as test from "${file}"`,
    'test.six',
  ]), [
    `import * as test from "${file}"\r`,
    'undefined',
    PROMPT,
    'test.six\r',
    '6',
    PROMPT
  ]);

  assert.deepStrictEqual(await runEach([
    `import { i, increment } from "${file}"`,
    'i',
    'increment()',
    'i',
  ]), [
    `import { i, increment } from "${file}"\r`,
    'undefined',
    PROMPT,
    'i\r',
    '0',
    PROMPT,
    'increment()\r',
    'undefined',
    PROMPT,
    'i\r',
    '1',
    PROMPT,
  ]);
}

main();
