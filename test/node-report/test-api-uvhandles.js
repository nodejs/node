'use strict';

// Testcase to check reporting of uv handles.
const common = require('../common');
common.skipIfReportDisabled();
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
  udp_socket.bind({});

  // Simple server/connection to create tcp uv handles.
  const server = http.createServer((req, res) => {
    req.on('end', () => {
      // Generate the report while the connection is active.
      console.log(process.report.getReport());
      child_process.kill();

      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end();

      // Tidy up to allow process to exit cleanly.
      server.close(() => {
        if (watcher) watcher.close();
        fs.unwatchFile(__filename);
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
                   skip_fs_watch: (watcher === undefined ?
                     'fs.watch() unavailable' :
                     false) };
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
  const child = fork('--experimental-report', [__filename, 'child'], options);
  let stderr = '';
  child.stderr.on('data', (chunk) => { stderr += chunk; });
  let stdout = '';
  const std_msg = 'Found messages in stderr unexpectedly: ';
  const report_msg = 'Report files were written: unexpectedly';
  child.stdout.on('data', (chunk) => { stdout += chunk; });
  child.on('exit', common.mustCall((code, signal) => {
    assert.deepStrictEqual(code, 0, 'Process exited unexpectedly with code' +
                           `${code}`);
    assert.deepStrictEqual(signal, null, 'Process should have exited cleanly,' +
                            ` but did not: ${signal}`);
    assert.ok(stderr.match(
      '(node:.*) ExperimentalWarning: report is an experimental' +
      ' feature. This feature could change at any time'),
              std_msg);

    const reports = helper.findReports(child.pid, tmpdir.path);
    assert.deepStrictEqual(reports, [], report_msg, reports);

    const report = JSON.parse(stdout);
    let fs = 0;
    let poll = 0;
    let process = 0;
    let timer = 0;
    let pipe = 0;
    let tcp = 0;
    let udp = 0;
    const fs_msg = 'fs_event not found';
    const poll_msg = 'poll_event not found';
    const process_msg = 'process event not found';
    const timer_msg = 'timer event not found';
    const pipe_msg = 'pipe event not found';
    const tcp_msg = 'tcp event not found';
    const udp_msg = 'udp event not found';
    for (const entry in report.libuv) {
      if (report.libuv[entry].type === 'fs_event') fs = 1;
      else if (report.libuv[entry].type === 'fs_poll') poll = 1;
      else if (report.libuv[entry].type === 'process') process = 1;
      else if (report.libuv[entry].type === 'timer') timer = 1;
      else if (report.libuv[entry].type === 'pipe') pipe = 1;
      else if (report.libuv[entry].type === 'tcp') tcp = 1;
      else if (report.libuv[entry].type === 'udp') udp = 1;
    }
    assert.deepStrictEqual(fs, 1, fs_msg);
    assert.deepStrictEqual(poll, 1, poll_msg);
    assert.deepStrictEqual(process, 1, process_msg);
    assert.deepStrictEqual(timer, 1, timer_msg);
    assert.deepStrictEqual(pipe, 1, pipe_msg);
    assert.deepStrictEqual(tcp, 1, tcp_msg);
    assert.deepStrictEqual(udp, 1, udp_msg);

    // Common report tests.
    helper.validateContent(stdout);
  }));
}
