'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const Repl = require('repl');

common.skipIfInspectorDisabled();

const PROMPT = 'repl > ';

class REPLStream extends ArrayStream {
  constructor() {
    super();
    this.lines = [''];
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
}

function runAndWait(cmds, repl) {
  const promise = repl.inputStream.wait();
  for (const cmd of cmds) {
    repl.inputStream.run([cmd]);
  }
  return promise;
}

async function tests(options) {
  const repl = Repl.start({
    prompt: PROMPT,
    stream: new REPLStream(),
    ignoreUndefined: true,
    useColors: true,
    ...options
  });

  repl.inputStream.run([
    'function foo(x) { return x; }',
    'function koo() { console.log("abc"); }',
    'a = undefined;'
  ]);
  const testCases = [
    ['foo', [2, 4], '[Function: foo]',
     'foo',
     '\x1B[90m[Function: foo]\x1B[39m\x1B[1A\x1B[11G\x1B[1B\x1B[2K\x1B[1A\r',
     '\x1B[36m[Function: foo]\x1B[39m',
     '\x1B[1G\x1B[0Jrepl > \x1B[8G'],
    ['koo', [2, 4], '[Function: koo]',
     'k\x1B[90moo\x1B[39m\x1B[9G\x1B[0Ko\x1B[90mo\x1B[39m\x1B[10G\x1B[0Ko',
     '\x1B[90m[Function: koo]\x1B[39m\x1B[1A\x1B[11G\x1B[1B\x1B[2K\x1B[1A\r',
     '\x1B[36m[Function: koo]\x1B[39m',
     '\x1B[1G\x1B[0Jrepl > \x1B[8G'],
    ['a', [1, 2], undefined],
    ['{ a: true }', [2, 3], '{ a: \x1B[33mtrue\x1B[39m }',
     '{ a: tru\x1B[90me\x1B[39m\x1B[16G\x1B[0Ke }\r',
     '{ a: \x1B[33mtrue\x1B[39m }',
     '\x1B[1G\x1B[0Jrepl > \x1B[8G'],
    ['1n + 2n', [2, 5], '\x1B[33m3n\x1B[39m',
     '1n + 2',
     '\x1B[90mType[39m\x1B[1A\x1B[14G\x1B[1B\x1B[2K\x1B[1An',
     '\x1B[90m3n\x1B[39m\x1B[1A\x1B[15G\x1B[1B\x1B[2K\x1B[1A\r',
     '\x1B[33m3n\x1B[39m',
     '\x1B[1G\x1B[0Jrepl > \x1B[8G'],
    ['{ a: true };', [2, 4], '\x1B[33mtrue\x1B[39m',
     '{ a: tru\x1B[90me\x1B[39m\x1B[16G\x1B[0Ke };',
     '\x1B[90mtrue\x1B[39m\x1B[1A\x1B[20G\x1B[1B\x1B[2K\x1B[1A\r',
     '\x1B[33mtrue\x1B[39m',
     '\x1B[1G\x1B[0Jrepl > \x1B[8G'],
    [' \t { a: true};', [2, 5], '\x1B[33mtrue\x1B[39m',
     ' \t { a: tru\x1B[90me\x1B[39m\x1B[19G\x1B[0Ke}',
     '\x1B[90m{ a: true }\x1B[39m\x1B[1A\x1B[21G\x1B[1B\x1B[2K\x1B[1A;',
     '\x1B[90mtrue\x1B[39m\x1B[1A\x1B[22G\x1B[1B\x1B[2K\x1B[1A\r',
     '\x1B[33mtrue\x1B[39m',
     '\x1B[1G\x1B[0Jrepl > \x1B[8G']
  ];

  const hasPreview = repl.terminal &&
    (options.preview !== undefined ? !!options.preview : true);

  for (const [input, length, expected, ...preview] of testCases) {
    console.log(`Testing ${input}`);

    const toBeRun = input.split('\n');
    let lines = await runAndWait(toBeRun, repl);

    assert.strictEqual(lines.length, length[+hasPreview]);
    if (expected === undefined) {
      assert(!lines.some((e) => e.includes('undefined')));
    } else if (hasPreview) {
      // Remove error messages. That allows the code to run in different
      // engines.
      // eslint-disable-next-line no-control-regex
      lines = lines.map((line) => line.replace(/Error: .+?\x1B/, ''));
      assert.deepStrictEqual(lines, preview);
    } else {
      assert.ok(lines[0].includes(expected), lines);
    }
  }
}

tests({ terminal: false }); // No preview
tests({ terminal: true }); // Preview
tests({ terminal: false, preview: false }); // No preview
tests({ terminal: false, preview: true }); // No preview
tests({ terminal: true, preview: true }); // Preview
