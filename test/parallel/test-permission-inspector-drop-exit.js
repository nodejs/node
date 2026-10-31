'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawn } = require('child_process');

function runTest(script) {
  return new Promise((resolve) => {
    const child = spawn(
      process.execPath,
      [
        '--permission',
        '--allow-inspector',
        '--allow-fs-read=*',
        '--allow-fs-write=*',
        '--inspect-wait=127.0.0.1:0',
        '-e',
        script,
      ],
      { stdio: ['ignore', 'pipe', 'pipe'] }
    );

    let stderr = '';
    let socket;
    let nextId = 0;

    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (chunk) => {
      stderr += chunk;
      const endpoint = stderr.match(
        /Debugger listening on (ws:\/\/127\.0\.0\.1:\d+\/\S+)/
      )?.[1];

      if (endpoint && !socket) {
        socket = new WebSocket(endpoint);
        socket.onopen = () => {
          for (const method of [
            'Debugger.enable',
            'Runtime.enable',
            'Runtime.runIfWaitingForDebugger',
          ]) {
            socket.send(JSON.stringify({ id: ++nextId, method }));
          }
        };
      }

      if (stderr.includes('Waiting for the debugger to disconnect')) {
        socket?.close();
      }
    });

    child.on('close', common.mustCall((code, signal) => {
      assert.strictEqual(
        code,
        0,
        `Process exited with code ${code}, signal ${signal}, stderr: ${stderr}`
      );
      assert.strictEqual(signal, null, `Process killed with signal ${signal}`);
      assert(
        !stderr.includes('FATAL ERROR'),
        `FATAL ERROR present in stderr: ${stderr}`
      );
      assert(
        !stderr.includes('Cannot create a handle without a HandleScope'),
        `HandleScope fatal present in stderr: ${stderr}`
      );
      resolve();
    }));
  });
}

async function main() {
  await runTest("process.permission.drop('inspector');");
  await runTest("process.permission.drop('inspector'); process.exit(0);");
}

main().then(common.mustCall());
