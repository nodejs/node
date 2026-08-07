'use strict';
const { isWindows } = require('../common');
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { createPipe } = require('node:net');
const { test } = require('node:test');
const { text } = require('node:stream/consumers');

// This test sketches how bash idioms could be expressed from JavaScript with
// process-owned pipe endpoints. The helpers between this comment and the test
// are small stubs that make the example executable; the test body is the part
// intended to demonstrate the user-facing syntax.

function waitForClose(child) {
  return once(child, 'close').then(([code, signal]) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
}

function commandFromTemplate(strings, values) {
  const args = [];
  for (let i = 0; i < strings.length; i++) {
    const words = strings[i].trim().split(/\s+/).filter(Boolean);
    Array.prototype.push.apply(args, words);
    if (i < values.length)
      args.push(String(values[i]));
  }

  const cmd = args.shift();
  return { cmd, args };
}

const curlStub = `
  const lines = [
    'event: log\\n',
    'data: alpha\\n',
    '\\n',
    'event: log\\n',
    'data: beta\\n',
    '\\n',
    'event: end\\n',
    '\\n',
  ];

  function writeLine() {
    const line = lines.shift();
    if (line == null)
      return;

    process.stdout.write(line);
    setTimeout(writeLine, 20);
  }

  writeLine();
`;

function startCommand(command, { stdin = 'ignore', stdout = 'pipe' } = {}) {
  const cmd = command.cmd === 'curl' ? process.execPath : command.cmd;
  const args = command.cmd === 'curl' ? ['-e', curlStub] : command.args;

  return spawn(cmd, args, {
    stdio: [stdin, stdout, 'inherit'],
  });
}

function createSubprocess(child, stdout) {
  const close = waitForClose(child);
  const read = stdout == null ? null : text(stdout);

  return {
    async close() {
      await close;
    },

    async read() {
      assert.notStrictEqual(read, null);
      const result = await read;
      await close;
      return result;
    },

    async readLine() {
      const result = await this.read();
      return result.split(/\r?\n/, 1)[0];
    },
  };
}

function runCommand(command, options = {}) {
  const capture = options.stdout == null;
  const child = startCommand(command, {
    ...options,
    stdout: capture ? 'pipe' : options.stdout,
  });

  return createSubprocess(child, capture ? child.stdout : null);
}

function runPipeline(commands, options = {}) {
  const capture = options.stdout == null;
  const stdout = capture ? createPipe() : null;
  let stdin = options.stdin;
  const children = [];
  const pipes = [];

  for (let i = 0; i < commands.length; i++) {
    const last = i === commands.length - 1;
    const pipe = last ? null : createPipe();
    const child = startCommand(commands[i], {
      stdin,
      stdout: last ? (capture ? stdout.writable : options.stdout) :
        pipe.writable,
    });

    children.push(child);
    if (pipe != null)
      pipes.push(pipe);
    stdin = pipe?.readable;
  }

  for (const pipe of pipes)
    pipe.writable.end();
  if (capture)
    stdout.writable.end();

  const close = Promise.all(children.map(waitForClose));
  const read = capture ? text(stdout.readable) : null;

  return {
    async close() {
      await close;
    },

    async read() {
      assert.notStrictEqual(read, null);
      const result = await read;
      await close;
      return result;
    },

    async readLine() {
      const result = await this.read();
      return result.split(/\r?\n/, 1)[0];
    },
  };
}

function $(strings, ...values) {
  if (Array.isArray(strings.raw)) {
    return command(strings, values);
  }

  const commands = [strings, ...values].map((command) => command.command);
  return (options) => runPipeline(commands, options);
}

function command(strings, values) {
  const command = commandFromTemplate(strings, values);
  const fn = (options) => runCommand(command, options);
  fn.command = command;
  return fn;
}

$.writer = function writer(command) {
  const pipe = createPipe();
  const child = startCommand(command.command, {
    stdout: pipe.writable,
  });

  child.once('close', () => pipe.writable.end());
  pipe.close = () => Promise.all([
    waitForClose(child),
    text(pipe.readable),
  ]);

  return pipe.readable;
};

if (!isWindows) test('bashjs-style pipeline leases readable endpoint',
  async () => {
    const events = $.writer(
      $`curl -N ${'https://example.com/events'}`
    );
    const logs = [];

    while (true) {
      // Read only the event type line, leaving the rest of the event in the
      // stream for the selected handler.
      const type = await $`sed -n ${'1{s/^event: //p;q}'}`({
        stdin: events,
      }).readLine();

      switch (type) {
        case 'end':
          return assert.deepStrictEqual(logs, ['alpha', 'beta']);

        case 'log': {
          // Read through the blank line that terminates this event, select
          // data lines, and strip the "data: " prefix.
          const log = await $(
            $`sed -n ${'/^$/q; /^data: /p'}`,
            $`cut -d ${' '} ${'-f2-'}`
          )({
            stdin: events,
          }).readLine();
          logs.push(log);
          break;
        }
      }
    }
  });
