'use strict';
const common = require('../common');
if (!common.isSunOS)
  common.skip('no DTRACE support');

const assert = require('assert');
const os = require('os');

/*
 * Some functions to create a recognizable stack.
 */
const frames = [ 'stalloogle', 'bagnoogle', 'doogle' ];

const stalloogle = (str) => {
  global.expected = str;
  os.loadavg();
};

const bagnoogle = (arg0, arg1) => {
  stalloogle(`${arg0} is ${arg1} except that it is read-only`);
};

let done = false;

const doogle = () => {
  if (!done)
    setTimeout(doogle, 10);

  bagnoogle('The bfs command', '(almost) like ed(1)');
};

const spawn = require('child_process').spawn;

/*
 * We're going to use DTrace to stop us, gcore us, and set us running again
 * when we call getloadavg() -- with the implicit assumption that our
 * deepest function is the only caller of os.loadavg().
 */
const dtrace = spawn('dtrace', [ '-qwn', `syscall::getloadavg:entry/pid == ${
  process.pid}/{ustack(100, 8192); exit(0); }` ]);

let output = '';

dtrace.stderr.on('data', function(data) {
  console.log(`dtrace: ${data}`);
});

dtrace.stdout.on('data', function(data) {
  output += data;
});

dtrace.on('exit', function(code) {
  if (code !== 0) {
    console.error(`dtrace exited with code ${code}`);
    process.exit(code);
  }

  done = true;

  const sentinel = '(anon) as ';
  const lines = output.split('\n');

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    if (!line.includes(sentinel) || frames.length === 0)
      continue;

    const frame = line.substr(line.indexOf(sentinel) + sentinel.length);
    const top = frames.shift();

    assert(frame.startsWith(top),
           `unexpected frame where ${top} was expected`);
  }

  assert.strictEqual(frames.length, 0,
                     `did not find expected frame ${frames[0]}`);
  process.exit(0);
});

setTimeout(doogle, 10);
