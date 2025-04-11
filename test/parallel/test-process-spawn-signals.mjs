import '../common/index.mjs';
import { describe, it } from 'node:test';
import { once } from 'node:events';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';

let fixturePaths;

const fixtureContent = {
  'signal-handler.js': `
console.log('Signal handler process started');

// Set up signal handlers
process.on('SIGTERM', () => {
  console.log('SIGTERM received');
  process.send('sigterm-executed');
});

process.on('SIGINT', () => {
  console.log('SIGINT received');
  process.send('sigint-executed');
});

// Notify that we're ready to receive signals
process.stdin.resume();
console.log('Waiting for signals...');
process.send('ready');

// Handle shutdown message
process.on('message', (message) => {
  if (message === 'exit') {
    console.log('Exit message received, shutting down');
    process.exit(0);
  }
});
`
};

function refresh() {
  tmpdir.refresh();
  fixturePaths = Object.keys(fixtureContent)
    .reduce((acc, file) => ({ ...acc, [file]: tmpdir.resolve(file) }), {});
  Object.entries(fixtureContent)
    .forEach(([file, content]) => writeFileSync(fixturePaths[file], content));
}

describe('process spawn child signals test', () => {
  it('should handle SIGTERM correctly', async () => {
    refresh();

    const child = spawn(process.execPath,
                        [
                          fixturePaths['signal-handler.js'],
                        ],
                        {
                          encoding: 'utf8',
                          stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
                          cwd: tmpdir.path,
                        });

    let stdout = '';
    child.stdout.on('data', (data) => {
      stdout += data.toString();
    });

    // Wait for the child to be ready
    const [readyMessage] = await once(child, 'message');
    assert.strictEqual(readyMessage, 'ready');

    // Send SIGTERM signal
    child.kill('SIGTERM');

    // Wait for the signal to be processed
    const [message] = await once(child, 'message');
    assert.strictEqual(message, 'sigterm-executed');

    child.send('exit');
    await once(child, 'exit');

    assert.match(stdout, /Signal handler process started/);
    assert.match(stdout, /Waiting for signals/);
    assert.match(stdout, /SIGTERM received/);
  });

  it('should handle SIGINT correctly', async () => {
    refresh();

    const child = spawn(process.execPath,
                        [
                          fixturePaths['signal-handler.js'],
                        ],
                        {
                          encoding: 'utf8',
                          stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
                          cwd: tmpdir.path,
                        });

    let stdout = '';
    child.stdout.on('data', (data) => {
      stdout += data.toString();
    });

    // Wait for the child to be ready
    const [readyMessage] = await once(child, 'message');
    assert.strictEqual(readyMessage, 'ready');

    // Send SIGINT signal
    child.kill('SIGINT');

    // Wait for the signal to be processed
    const [message] = await once(child, 'message');
    assert.strictEqual(message, 'sigint-executed');

    child.send('exit');
    await once(child, 'exit');

    assert.match(stdout, /Signal handler process started/);
    assert.match(stdout, /Waiting for signals/);
    assert.match(stdout, /SIGINT received/);
  });
});
