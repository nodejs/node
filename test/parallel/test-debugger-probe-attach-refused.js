// Tests attach failures result in an error instead of a report. Without --pid the target
// should already be listening, so a failed connection should be an immediate error.
// An inspector that belongs to another process should be rejected.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const net = require('net');
const { once } = require('events');
const { runProbe, startTarget } = require('../common/debugger-probe');

async function main() {
  // Create a port that will refuse connections.
  const server = net.createServer((socket) => socket.resetAndDestroy());
  await once(server.listen(0, '127.0.0.1'), 'listening');
  const { port } = server.address();

  await runProbe(['--probe', 'app.js:1', '--expr', '1', '--port', `${port}`], {
    status: 1,
    report: null,
    stderr: new RegExp(`Could not attach to 127\\.0\\.0\\.1:${port}: `),
  });
  server.close();

  const other = await startTarget('probe-attach-loop.js');
  const target = await startTarget('probe-attach-loop.js', { args: ['--inspect-port=0'] });
  await runProbe(['--probe', 'app.js:1', '--expr', '1', '--pid', `${target.child.pid}`, '--port', `${other.port}`], {
    target,
    status: 1,
    report: null,
    stderr: new RegExp(`The inspector at 127\\.0\\.0\\.1:${other.port} belongs to process ${other.child.pid}, ` +
      `not to --pid ${target.child.pid}\\.`),
  });
  other.child.kill();
  target.child.kill();
}

main().then(common.mustCall());
