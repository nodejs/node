'use strict';

// Testcase to check reporting of uv handles.
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const path = require('path');
if (common.isIBMi)
  common.skip('IBMi does not support fs.watch()');

// This is quite similar to common.PIPE except that it uses an extended prefix
// of "\\?\pipe" on windows.
const PIPE = (() => {
  const localRelative = path.relative(process.cwd(), `${tmpdir.path}/`);
  const pipePrefix = common.isWindows ? '\\\\?\\pipe\\' : localRelative;
  const pipeName = `node-test.${process.pid}.sock`;
  return path.join(pipePrefix, pipeName);
})();

function createFsHandle(childData) {
  const fs = require('fs');
  // Watching files should result in fs_event/fs_poll uv handles.
  let watcher;
  try {
    watcher = fs.watch(__filename);
  } catch {
    // fs.watch() unavailable
  }
  fs.watchFile(__filename, () => {});
  childData.skip_fs_watch = watcher === undefined;

  return () => {
    if (watcher) watcher.close();
    fs.unwatchFile(__filename);
  };
}

function createChildProcessHandle(childData) {
  const spawn = require('child_process').spawn;
  // Child should exist when this returns as child_process.pid must be set.
  const cp = spawn(process.execPath,
                   ['-e', "process.stdin.on('data', (x) => " +
          'console.log(x.toString()));']);
  childData.pid = cp.pid;

  return () => {
    cp.kill();
  };
}

function createTimerHandle() {
  const timeout = setInterval(() => {}, 1000);
  // Make sure the timer doesn't keep the test alive and let
  // us check we detect unref'd handles correctly.
  timeout.unref();
  return () => {
    clearInterval(timeout);
  };
}

function createTcpHandle(childData) {
  const http = require('http');

  return new Promise((resolve) => {
    // Simple server/connection to create tcp uv handles.
    const server = http.createServer((req, res) => {
      req.on('end', () => {
        resolve(() => {
          res.writeHead(200, { 'Content-Type': 'text/plain' });
          res.end();
          server.close();
        });
      });
      req.resume();
    });
    server.listen(() => {
      childData.tcp_address = server.address();
      http.get({ port: server.address().port });
    });
  });
}

function createUdpHandle(childData) {
  // Datagram socket for udp uv handles.
  const dgram = require('dgram');
  const udpSocket = dgram.createSocket('udp4');
  const connectedUdpSocket = dgram.createSocket('udp4');

  return new Promise((resolve) => {
    udpSocket.bind({}, common.mustCall(() => {
      connectedUdpSocket.connect(udpSocket.address().port);

      childData.udp_address = udpSocket.address();
      resolve(() => {
        connectedUdpSocket.close();
        udpSocket.close();
      });
    }));
  });
}

function createNamedPipeHandle(childData) {
  const net = require('net');
  const sockPath = PIPE;
  return new Promise((resolve) => {
    const server = net.createServer((socket) => {
      childData.pipe_sock_path = server.address();
      resolve(() => {
        socket.end();
        server.close();
      });
    });
    server.listen(
      sockPath,
      () => {
        net.connect(sockPath, (socket) => {});
      });
  });
}

async function child() {
  // Exit on loss of parent process
  const exit = () => process.exit(2);
  process.on('disconnect', exit);

  const childData = {};
  const disposes = await Promise.all([
    createFsHandle(childData),
    createChildProcessHandle(childData),
    createTimerHandle(childData),
    createTcpHandle(childData),
    createUdpHandle(childData),
    createNamedPipeHandle(childData),
  ]);
  process.send(childData);

  // Generate the report while the connection is active.
  console.log(JSON.stringify(process.report.getReport(), null, 2));

  // Tidy up to allow process to exit cleanly.
  disposes.forEach((it) => {
    it();
  });
  process.removeListener('disconnect', exit);
}

if (process.argv[2] === 'child') {
  child();
} else {
  const helper = require('../common/report.js');
  const fork = require('child_process').fork;
  const assert = require('assert');
  tmpdir.refresh();
  const options = { encoding: 'utf8', silent: true, cwd: tmpdir.path };
  const child = fork(__filename, ['child'], options);
  let child_data;
  child.on('message', (data) => { child_data = data; });
  let stderr = '';
  child.stderr.on('data', (chunk) => { stderr += chunk; });
  let stdout = '';
  const report_msg = 'Report files were written: unexpectedly';
  child.stdout.on('data', (chunk) => { stdout += chunk; });
  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(stderr.trim(), '');
    assert.deepStrictEqual(code, 0, 'Process exited unexpectedly with code: ' +
                           `${code}`);
    assert.deepStrictEqual(signal, null, 'Process should have exited cleanly,' +
                            ` but did not: ${signal}`);

    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.deepStrictEqual(reports, [], report_msg, reports);

    // Test libuv handle key order
    {
      const get_libuv = /"libuv":\s\[([\s\S]*?)\]/g;
      const get_handle_inner = /{([\s\S]*?),*?}/g;
      const libuv_handles_str = get_libuv.exec(stdout)[1];
      const libuv_handles_array = libuv_handles_str.match(get_handle_inner);
      for (const i of libuv_handles_array) {
        // Exclude nested structure
        if (i.includes('type')) {
          const handle_keys = i.match(/(".*"):/g);
          assert(handle_keys[0], 'type');
          assert(handle_keys[1], 'is_active');
        }
      }
    }

    const report = JSON.parse(stdout);
    const prefix = common.isWindows ? '\\\\?\\' : '';
    const expected_filename = `${prefix}${__filename}`;
    const found_tcp = [];
    const found_udp = [];
    const found_named_pipe = [];
    // Functions are named to aid debugging when they are not called.
    const validators = {
      fs_event: common.mustCall(function fs_event_validator(handle) {
        if (!child_data.skip_fs_watch) {
          assert.strictEqual(handle.filename, expected_filename);
          assert(handle.is_referenced);
        }
      }),
      fs_poll: common.mustCall(function fs_poll_validator(handle) {
        assert.strictEqual(handle.filename, expected_filename);
        assert(handle.is_referenced);
      }),
      loop: common.mustCall(function loop_validator(handle) {
        assert.strictEqual(typeof handle.loopIdleTimeSeconds, 'number');
      }),
      pipe: common.mustCallAtLeast(function pipe_validator(handle) {
        assert(handle.is_referenced);
        // Pipe handles. The report should contain three pipes:
        // 1. The server's listening pipe.
        // 2. The inbound pipe making the request.
        // 3. The outbound pipe sending the response.
        //
        // There is no way to distinguish inbound and outbound in a cross
        // platform manner, so we just check inbound here.
        const sockPath = child_data.pipe_sock_path;
        if (handle.localEndpoint === sockPath) {
          if (handle.writable === false) {
            found_named_pipe.push('listening');
          }
        } else if (handle.remoteEndpoint === sockPath) {
          found_named_pipe.push('inbound');
        }
      }),
      process: common.mustCall(function process_validator(handle) {
        assert.strictEqual(handle.pid, child_data.pid);
        assert(handle.is_referenced);
      }),
      tcp: common.mustCall(function tcp_validator(handle) {
        // TCP handles. The report should contain three sockets:
        // 1. The server's listening socket.
        // 2. The inbound socket making the request.
        // 3. The outbound socket sending the response.
        const port = child_data.tcp_address.port;
        if (handle.localEndpoint.port === port) {
          if (handle.remoteEndpoint === null) {
            found_tcp.push('listening');
          } else {
            found_tcp.push('inbound');
          }
        } else if (handle.remoteEndpoint.port === port) {
          found_tcp.push('outbound');
        }
        assert(handle.is_referenced);
      }, 3),
      timer: common.mustCallAtLeast(function timer_validator(handle) {
        assert(!handle.is_referenced);
        assert.strictEqual(handle.repeat, 0);
      }),
      udp: common.mustCall(function udp_validator(handle) {
        if (handle.remoteEndpoint === null) {
          assert.strictEqual(handle.localEndpoint.port,
                             child_data.udp_address.port);
          found_udp.push('unconnected');
        } else {
          assert.strictEqual(handle.remoteEndpoint.port,
                             child_data.udp_address.port);
          found_udp.push('connected');
        }
        assert(handle.is_referenced);
      }, 2),
    };

    for (const entry of report.libuv) {
      if (validators[entry.type]) validators[entry.type](entry);
    }
    for (const socket of ['listening', 'inbound', 'outbound']) {
      assert(found_tcp.includes(socket), `${socket} TCP socket was not found`);
    }
    for (const socket of ['connected', 'unconnected']) {
      assert(found_udp.includes(socket), `${socket} UDP socket was not found`);
    }
    for (const socket of ['listening', 'inbound']) {
      assert(found_named_pipe.includes(socket), `${socket} named pipe socket was not found`);
    }

    // Common report tests.
    helper.validateContent(stdout);
  }));
}
