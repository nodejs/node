'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const CODE =
  'setTimeout(() => { for (let i = 0; i < 100000; i++) { "test" + i } }, 1);' +
  'process.title = "foo"';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const FILE_NAME = tmpdir.resolve('node_trace.1.log');

const proc = cp.spawn(process.execPath,
                      [ '--trace-event-categories', 'node.perf.usertiming',
                        '--title=bar',
                        '-e', CODE ],
                      { cwd: tmpdir.path });
proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents
      .filter((trace) => trace.cat === '__metadata');
    assert(traces.length > 0);
    assert(traces.some((trace) =>
      trace.name === 'thread_name' &&
        trace.args.name === 'JavaScriptMainThread'));
    assert(traces.some((trace) =>
      trace.name === 'thread_name' &&
        trace.args.name === 'PlatformWorkerThread'));
    assert(traces.some((trace) =>
      trace.name === 'version' &&
        trace.args.node === process.versions.node));

    assert(traces.some((trace) =>
      trace.name === 'node' &&
        trace.args.process.versions.http_parser ===
          process.versions.http_parser &&
        trace.args.process.versions.llhttp ===
          process.versions.llhttp &&
        trace.args.process.versions.node ===
          process.versions.node &&
        trace.args.process.versions.v8 ===
          process.versions.v8 &&
        trace.args.process.versions.uv ===
          process.versions.uv &&
        trace.args.process.versions.zlib ===
          process.versions.zlib &&
        trace.args.process.versions.ares ===
          process.versions.ares &&
        trace.args.process.versions.modules ===
          process.versions.modules &&
        trace.args.process.versions.nghttp2 ===
          process.versions.nghttp2 &&
        trace.args.process.versions.napi ===
          process.versions.napi &&
        trace.args.process.versions.openssl ===
          process.versions.openssl &&
        trace.args.process.arch === process.arch &&
        trace.args.process.platform === process.platform &&
        trace.args.process.release.name === process.release.name &&
        (!process.release.lts ||
          trace.args.process.release.lts === process.release.lts)));

    if (!common.isSunOS && !common.isIBMi) {
      // Changing process.title is currently unsupported on SunOS/SmartOS
      // and IBMi
      assert(traces.some((trace) =>
        trace.name === 'process_name' && trace.args.name === 'foo'));
      assert(traces.some((trace) =>
        trace.name === 'process_name' && trace.args.name === 'bar'));
    }
  }));
}));
