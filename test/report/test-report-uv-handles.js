'use strict';

// Testcase to check reporting of uv handles.
const common = require('../common');
if (common.isIBMi)
  common.skip('IBMi does not support fs.watch()');

if (process.argv[2] === 'child') {
  // Exit on loss of parent process
  const exit = () => process.exit(2);
  process.on('disconnect', exit);

  const fs = require('fs');
  const http = require('http');
  const spawn = require('child_process').spawn;

  // Watching files should result in fs_event/fs_poll uv handles.
  let watcher;
  try {
    watcher = fs.watch(__filename);
  } catch {
    // fs.watch() unavailable
  }
  fs.watchFile(__filename, () => {});

  // Child should exist when this returns as child_process.pid must be set.
  const child_process = spawn(process.execPath,
                              ['-e', "process.stdin.on('data', (x) => " +
                                     'console.log(x.toString()));']);

  const timeout = setInterval(() => {}, 1000);
  // Make sure the timer doesn't keep the test alive and let
  // us check we detect unref'd handles correctly.
  timeout.unref();

  // Datagram socket for udp uv handles.
  const dgram = require('dgram');
  const udp_socket = dgram.createSocket('udp4');
  const connected_udp_socket = dgram.createSocket('udp4');
  udp_socket.bind({}, common.mustCall(() => {
    connected_udp_socket.connect(udp_socket.address().port);
  }));

  // Simple server/connection to create tcp uv handles.
  const server = http.createServer((req, res) => {
    req.on('end', () => {
      // Generate the report while the connection is active.
      console.log(JSON.stringify(process.report.getReport(), null, 2));
      child_process.kill();

      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end();

      // Tidy up to allow process to exit cleanly.
      server.close(() => {
        if (watcher) watcher.close();
        fs.unwatchFile(__filename);
        connected_udp_socket.close();
        udp_socket.close();
        process.removeListener('disconnect', exit);
      });
    });
    req.resume();
  });
  server.listen(() => {
    const data = { pid: child_process.pid,
                   tcp_address: server.address(),
                   udp_address: udp_socket.address(),
                   skip_fs_watch: (watcher === undefined) };
    process.send(data);
    http.get({ port: server.address().port });
  });
} else {
  const helper = require('../common/report.js');
  const fork = require('child_process').fork;
  const assert = require('assert');
  const tmpdir = require('../common/tmpdir');
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
    assert.deepStrictEqual(code, 0, 'Process exited unexpectedly with code: ' +
                           `${code}`);
    assert.deepStrictEqual(signal, null, 'Process should have exited cleanly,' +
                            ` but did not: ${signal}`);
    assert.strictEqual(stderr.trim(), '');

    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.deepStrictEqual(reports, [], report_msg, reports);

    // Test libuv handle key order
    {
      const get_libuv = /"libuv":\s\[([\s\S]*?)\]/gm;
      const get_handle_inner = /{([\s\S]*?),*?}/gm;
      const libuv_handles_str = get_libuv.exec(stdout)[1];
      const libuv_handles_array = libuv_handles_str.match(get_handle_inner);
      for (const i of libuv_handles_array) {
        // Exclude nested structure
        if (i.includes('type')) {
          const handle_keys = i.match(/(".*"):/gm);
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
      pipe: common.mustCallAtLeast(function pipe_validator(handle) {
        assert(handle.is_referenced);
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
    console.log(report.libuv);
    for (const entry of report.libuv) {
      if (validators[entry.type]) validators[entry.type](entry);
    }
    for (const socket of ['listening', 'inbound', 'outbound']) {
      assert(found_tcp.includes(socket), `${socket} TCP socket was not found`);
    }
    for (const socket of ['connected', 'unconnected']) {
      assert(found_udp.includes(socket), `${socket} UDP socket was not found`);
    }

    // Common report tests.
    helper.validateContent(stdout);
  }));
}
