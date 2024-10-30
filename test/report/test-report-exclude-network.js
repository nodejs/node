'use strict';
require('../common');
const http = require('node:http');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const tmpdir = require('../common/tmpdir');
const { describe, it, before } = require('node:test');
const fs = require('node:fs');
const helper = require('../common/report');

function validate(pid) {
  const reports = helper.findReports(pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);
  let report = fs.readFileSync(reports[0], { encoding: 'utf8' });
  report = JSON.parse(report);
  assert.strictEqual(report.header.networkInterfaces, undefined);
  fs.unlinkSync(reports[0]);
}

describe('report exclude network option', () => {
  before(() => {
    tmpdir.refresh();
    process.report.directory = tmpdir.path;
  });

  it('should be configurable with --report-exclude-network', () => {
    const args = ['--report-exclude-network', '-e', 'process.report.writeReport()'];
    const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
    assert.strictEqual(child.status, 0);
    assert.strictEqual(child.signal, null);
    validate(child.pid);
  });

  it('should be configurable with report.excludeNetwork', () => {
    process.report.excludeNetwork = true;
    process.report.writeReport();
    validate(process.pid);

    const report = process.report.getReport();
    assert.strictEqual(report.header.networkInterfaces, undefined);
  });

  it('should not do DNS queries in libuv if exclude network', async () => {
    const server = http.createServer(function(req, res) {
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end();
    });
    const port = await new Promise((resolve) => server.listen(0, async () => {
      await Promise.all([
        fetch('http://127.0.0.1:' + server.address().port),
        fetch('http://[::1]:' + server.address().port),
      ]);
      resolve(server.address().port);
      server.close();
    }));
    process.report.excludeNetwork = false;
    let report = process.report.getReport();
    let tcp = report.libuv.filter((uv) => uv.type === 'tcp' && uv.remoteEndpoint?.port === port);
    assert.strictEqual(tcp.length, 2);
    const findHandle = (host, local = true, ip4 = true) => {
      return tcp.some(
        ({ [local ? 'localEndpoint' : 'remoteEndpoint']: ep }) =>
          (ep[ip4 ? 'ip4' : 'ip6'] === (ip4 ? '127.0.0.1' : '::1') &&
            (Array.isArray(host) ? host.includes(ep.host) : ep.host === host)),
      );
    };
    try {
      assert.ok(findHandle('localhost'), 'local localhost handle not found');
      assert.ok(findHandle('localhost', false), 'remote localhost handle not found');

      assert.ok(findHandle(['localhost', 'ip6-localhost'], true, false), 'local ip6-localhost handle not found');
      assert.ok(findHandle(['localhost', 'ip6-localhost'], false, false), 'remote ip6-localhost handle not found');
    } catch (e) {
      throw new Error(e.message + ' in ' + JSON.stringify(tcp, null, 2));
    }

    process.report.excludeNetwork = true;
    report = process.report.getReport();
    tcp = report.libuv.filter((uv) => uv.type === 'tcp' && uv.remoteEndpoint?.port === port);

    try {
      assert.strictEqual(tcp.length, 2);
      assert.ok(findHandle('127.0.0.1'), 'local 127.0.0.1 handle not found');
      assert.ok(findHandle('127.0.0.1', false), 'remote 127.0.0.1 handle not found');

      assert.ok(findHandle('::1', true, false), 'local ::1 handle not found');
      assert.ok(findHandle('::1', false, false), 'remote ::1 handle not found');
    } catch (e) {
      throw new Error(e.message + ' in ' + JSON.stringify(tcp, null, 2));
    }
  });
});
