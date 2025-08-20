'use strict';

// This tests the creation of a single executable application that can be
// debugged using the inspector protocol with NODE_OPTIONS=--inspect-brk=0

require('../common');
const assert = require('assert');
const { writeFileSync, existsSync } = require('fs');
const { spawn } = require('child_process');
const tmpdir = require('../common/tmpdir');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

const configFile = tmpdir.resolve('sea-config.json');
const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();

// Create a simple hello world script
writeFileSync(tmpdir.resolve('hello.js'), `console.log('Hello, world!');`, 'utf-8');

// Create SEA configuration
writeFileSync(configFile, `
{
  "main": "hello.js",
  "output": "sea-prep.blob"
}
`);

// Generate the SEA prep blob
spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  { cwd: tmpdir.path }
);

assert(existsSync(seaPrepBlob));

// Generate the SEA executable
generateSEA(outputFile, process.execPath, seaPrepBlob, true);

// Spawn the SEA with inspect option
const seaProcess = spawn(outputFile, [], {
  env: {
    ...process.env,
    NODE_OPTIONS: '--inspect-brk=0',
  },
});

let debuggerUrl = null;
let seaStderr = '';

seaProcess.stderr.setEncoding('utf8');
seaProcess.stdout.setEncoding('utf8');

seaProcess.stdout.on('data', (data) => {
  console.log(`[SEA][STDOUT] ${data}`);
});

seaProcess.stderr.on('data', (data) => {
  console.log(`[SEA][STDERR] ${data}`);
  seaStderr += data;

  // Parse the debugger listening message
  const match = seaStderr.match(/Debugger listening on ws:\/\/([\d.]+):(\d+)\//);
  if (match && !debuggerUrl) {
    const host = match[1];
    const port = match[2];
    debuggerUrl = `${host}:${port}`;

    console.log(`Running ${process.execPath} inspect ${debuggerUrl}`);
    // Once we have the debugger URL, spawn the inspector CLI
    const inspectorProcess = spawn(process.execPath, ['inspect', debuggerUrl], {
      stdio: ['pipe', 'pipe', 'pipe'],
    });

    let inspectorStdout = '';
    inspectorProcess.stdout.setEncoding('utf8');
    inspectorProcess.stderr.setEncoding('utf8');

    inspectorProcess.stdout.on('data', (data) => {
      console.log(`[INSPECT][STDOUT] ${data}`);
      inspectorStdout += data;

      // Check if we successfully connected
      const matches = [...inspectorStdout.matchAll(/debug> /g)];
      if (inspectorStdout.includes(`connecting to ${host}:${port} ... ok`) &&
          matches.length >= 2) {
        // We are at the second prompt, which means we can send commands to terminate both now.
        console.log('Sending .exit command to inspector...');
        inspectorProcess.stdin.write('.exit\n');
      }
    });

    inspectorProcess.stderr.on('data', (data) => {
      console.log(`[INSPECT][STDERR] ${data}`);
    });

    inspectorProcess.on('close', (code) => {
      assert.strictEqual(code, 0, `Inspector process exited with code ${code}.`);
    });

    inspectorProcess.on('error', (err) => {
      throw err;
    });
  }
});

seaProcess.on('close', (code) => {
  assert.strictEqual(code, 0, `SEA process exited with code ${code}.`);
});

seaProcess.on('error', (err) => {
  throw err;
});
