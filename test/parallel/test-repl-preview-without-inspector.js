'use strict';

const common = require('../common');
const assert = require('assert');
const { REPLServer } = require('repl');
const { Stream } = require('stream');

if (process.features.inspector)
  common.skip('test is for node compiled with --without-inspector only');

// Ignore terminal settings. This is so the test can be run intact if TERM=dumb.
process.env.TERM = '';
const PROMPT = 'repl > ';

class REPLStream extends Stream {
  readable = true;
  writable = true;

  constructor() {
    super();
    this.lines = [''];
  }
  run(data) {
    for (const entry of data) {
      this.emit('data', entry);
    }
    this.emit('data', '\n');
  }
  write(chunk) {
    const chunkLines = chunk.toString('utf8').split('\n');
    this.lines[this.lines.length - 1] += chunkLines[0];
    if (chunkLines.length > 1) {
      this.lines.push(...chunkLines.slice(1));
    }
    this.emit('line');
    return true;
  }
  wait() {
    this.lines = [''];
    return new Promise((resolve, reject) => {
      const onError = (err) => {
        this.removeListener('line', onLine);
        reject(err);
      };
      const onLine = () => {
        if (this.lines[this.lines.length - 1].includes(PROMPT)) {
          this.removeListener('error', onError);
          this.removeListener('line', onLine);
          resolve(this.lines);
        }
      };
      this.once('error', onError);
      this.on('line', onLine);
    });
  }
  pause() { }
  resume() { }
}

function runAndWait(cmds, repl) {
  const promise = repl.inputStream.wait();
  for (const cmd of cmds) {
    repl.inputStream.run(cmd);
  }
  return promise;
}

const repl = REPLServer({
  prompt: PROMPT,
  stream: new REPLStream(),
  ignoreUndefined: true,
  useColors: true,
  terminal: true,
});

repl.inputStream.run([
  'function foo(x) { return x; }',
  'function koo() { console.log("abc"); }',
  'a = undefined;',
  'const r = 5;',
]);

/* eslint-disable @stylistic/js/max-len */
const testCases = [{
  input: 'foo',
  preview: [
    '\x1B[1G\x1B[0Jrepl > f\x1B[9G\x1B[1G\x1B[0Jrepl > fo\x1B[10G\x1B[1G\x1B[0Jrepl > foo\x1B[11G\r',
    '\x1B[36m[Function: foo]\x1B[39m',
  ]
}, {
  input: 'koo',
  preview: [
    '\x1B[1G\x1B[0Jrepl > k\x1B[9G\x1B[1G\x1B[0Jrepl > ko\x1B[10G\x1B[1G\x1B[0Jrepl > koo\x1B[11G\r',
    '\x1B[36m[Function: koo]\x1B[39m',
  ]
}, {
  input: 'a',
  noPreview: 'repl > ', // No "undefined" output.
  preview: ['\x1B[1G\x1B[0Jrepl > a\x1B[9G\r'] // No "undefined" preview.
}, {
  input: " { b: 1 }['b'] === 1",
  preview: [
    "\x1B[1G\x1B[0Jrepl >  \x1B[9G\x1B[1G\x1B[0Jrepl >  {\x1B[10G\x1B[1G\x1B[0Jrepl >  { \x1B[11G\x1B[1G\x1B[0Jrepl >  { b\x1B[12G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m\x1B[12G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[13G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m\x1B[12G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m \x1B[12G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }\x1B[12G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[13G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'\x1B[39m\x1B[13G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b\x1B[39m\x1B[14G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m\x1B[13G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m]\x1B[13G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[13G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m=\x1B[39m\x1B[14G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m==\x1B[39m\x1B[14G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m===\x1B[39m\x1B[15G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m===\x1B[39m \x1B[14G\x1B[1G\x1B[0Jrepl >  { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m===\x1B[39m \x1B[33m1\x1B[39m\x1B[14G\r",
    '\x1B[33mtrue\x1B[39m',
  ]
}, {
  input: "{ b: 1 }['b'] === 1;",
  preview: [
    "\x1B[1G\x1B[0Jrepl > {\x1B[9G\x1B[1G\x1B[0Jrepl > { \x1B[10G\x1B[1G\x1B[0Jrepl > { b\x1B[11G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m\x1B[11G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[12G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m\x1B[11G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m \x1B[11G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }\x1B[11G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[12G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'\x1B[39m\x1B[12G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b\x1B[39m\x1B[13G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m\x1B[12G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m]\x1B[12G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[12G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m=\x1B[39m\x1B[13G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m==\x1B[39m\x1B[13G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m===\x1B[39m\x1B[14G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m===\x1B[39m \x1B[13G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m===\x1B[39m \x1B[33m1\x1B[39m\x1B[13G\x1B[1G\x1B[0Jrepl > { b\x1B[36m:\x1B[39m \x1B[33m1\x1B[39m }[\x1B[32m'b'\x1B[39m] \x1B[36m===\x1B[39m \x1B[33m1\x1B[39m;\x1B[13G\r",
    '\x1B[33mfalse\x1B[39m',
  ]
}, {
  input: '{ a: true }',
  preview: [
    '\x1B[1G\x1B[0Jrepl > {\x1B[9G\x1B[1G\x1B[0Jrepl > { \x1B[10G\x1B[1G\x1B[0Jrepl > { a\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[12G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m t\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m tr\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m tru\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m\x1B[12G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m \x1B[12G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m }\x1B[13G\r',
    '{ a: \x1B[33mtrue\x1B[39m }',
  ]
}, {
  input: '{ a: true };',
  preview: [
    '\x1B[1G\x1B[0Jrepl > {\x1B[9G\x1B[1G\x1B[0Jrepl > { \x1B[10G\x1B[1G\x1B[0Jrepl > { a\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[12G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m t\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m tr\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m tru\x1B[11G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m\x1B[12G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m \x1B[12G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m }\x1B[13G\x1B[1G\x1B[0Jrepl > { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m };\x1B[12G\r',
    '\x1B[33mtrue\x1B[39m',
  ]
}, {
  input: ' \t { a: true};',
  preview: [
    '\x1B[1G\x1B[0Jrepl >  \x1B[9G\x1B[1G\x1B[0Jrepl >   \x1B[10G\x1B[1G\x1B[0Jrepl >   {\x1B[11G\x1B[1G\x1B[0Jrepl >   { \x1B[12G\x1B[1G\x1B[0Jrepl >   { a\x1B[13G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m\x1B[13G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m \x1B[14G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m t\x1B[13G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m tr\x1B[13G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m tru\x1B[13G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m\x1B[14G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m}\x1B[14G\x1B[1G\x1B[0Jrepl >   { a\x1B[36m:\x1B[39m \x1B[33mtrue\x1B[39m};\x1B[15G\r',
    '\x1B[33mtrue\x1B[39m',
  ]
}, {
  input: '1n + 2n',
  preview: [
    '\x1B[1G\x1B[0Jrepl > \x1B[33m1\x1B[39m\x1B[8G\x1B[1G\x1B[0Jrepl > 1n\x1B[10G\x1B[1G\x1B[0Jrepl > 1n \x1B[11G\x1B[1G\x1B[0Jrepl > 1n \x1B[36m+\x1B[39m\x1B[11G\x1B[1G\x1B[0Jrepl > 1n \x1B[36m+\x1B[39m \x1B[12G\x1B[1G\x1B[0Jrepl > 1n \x1B[36m+\x1B[39m \x1B[33m2\x1B[39m\x1B[11G\x1B[1G\x1B[0Jrepl > 1n \x1B[36m+\x1B[39m 2n\x1B[11G\r',
    '\x1B[33m3n\x1B[39m',
  ]
}, {
  input: '{};1',
  preview: [
    '\x1B[1G\x1B[0Jrepl > {\x1B[9G\x1B[1G\x1B[0Jrepl > {}\x1B[10G\x1B[1G\x1B[0Jrepl > {};\x1B[11G\x1B[1G\x1B[0Jrepl > {};\x1B[33m1\x1B[39m\x1B[11G\r',
    '\x1B[33m1\x1B[39m',
  ]
}];
/* eslint-enable @stylistic/js/max-len */

async function runTest() {
  for (const { input, preview } of testCases) {
    const toBeRun = input.split('\n');
    let lines = await runAndWait(toBeRun, repl);
    // Remove error messages. That allows the code to run in different
    // engines.
    // eslint-disable-next-line no-control-regex
    lines = lines.map((line) => line.replace(/Error: .+?\x1B/, ''));
    assert.strictEqual(lines.pop(), '\x1B[1G\x1B[0Jrepl > \x1B[8G');
    assert.deepStrictEqual(lines, preview);
  }
}

runTest().then(common.mustCall());
