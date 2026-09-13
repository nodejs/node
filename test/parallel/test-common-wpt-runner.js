'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

if (process.env.NODE_TEST_WPT_REPORT_DIR) {
  const { WPTRunner } = require('../common/wpt');
  const runner = new WPTRunner('html/webappapis/atob');
  runner.report.filepath = path.join(process.env.NODE_TEST_WPT_REPORT_DIR, runner.report.filename);
  runner.runJsTests();
} else if (process.env.NODE_TEST_WPT_QUERY_PROBE) {
  const { WPTRunner, WPTTestSpec } = require('../common/wpt');
  const runner = new WPTRunner('compression');
  if (runner.managed?.mode === 'run') assert.strictEqual(runner.concurrency, 1);
  runner.specs = new Set(['?pass', '?fail'].map((query) => {
    const spec = new WPTTestSpec('compression', 'compression-bad-chunks.any.js', [], query, 'window');
    spec.failedTests = ['expected across queries'];
    if (process.env.NODE_TEST_WPT_QUERY_PROBE === 'flaky' ||
        (process.env.NODE_TEST_WPT_QUERY_PROBE === 'mixed' && query === '?pass')) {
      spec.flakyTests = [...spec.failedTests];
    }
    return spec;
  }));
  runner.setScriptModifier((script) => {
    if (!script.filename.endsWith('compression-bad-chunks.any.js')) return;
    script.code = `test(() => assert_true(${process.env.NODE_TEST_WPT_QUERY_PROBE === 'missing'} ||
      location.search === '?pass'), 'expected across queries');`;
  });
  runner.runJsTests();
} else {
  main();
}

function main() {
  tmpdir.refresh();
  const env = { ...process.env };
  for (const key of ['NODE_TEST_WPT', 'WPT_REPORT', 'WPT_INSPECT']) delete env[key];
  const driver = (name) => path.join(__dirname, '../wpt', `test-${name}.js`);
  function invoke(file, config, overrides = {}, status = 0) {
    const result = spawnSync(process.execPath, [file], {
      env: { ...env, ...overrides, NODE_TEST_WPT: JSON.stringify(config) },
      encoding: 'utf8', timeout: common.platformTimeout(10_000),
      maxBuffer: 10 * 1024 * 1024,
    });
    assert.ifError(result.error);
    assert.strictEqual(result.status, status, result.stdout + result.stderr);
    return result.stdout + result.stderr;
  }

  function discover(name, overrides, file = driver(name)) {
    const stdout = invoke(file, { mode: 'list' }, overrides);
    const lines = stdout.split('\n').filter((line) => line.startsWith('NODE_TEST_WPT_MANIFEST:'));
    assert.strictEqual(lines.length, 1);
    assert.doesNotMatch(stdout, /\[PASS\]/);
    const manifest = JSON.parse(lines[0].slice('NODE_TEST_WPT_MANIFEST:'.length));
    assert.strictEqual(manifest.version, 1);
    assert.strictEqual(new Set(manifest.tests.map((test) => test.id)).size, manifest.tests.length);
    return manifest;
  }

  const atob = discover('atob');
  assert.strictEqual(atob.serial, false);
  assert.deepStrictEqual(atob.tests.map((test) => test.id), ['base64.any.html', 'base64.any.worker.html']);
  const reportRoot = path.join(tmpdir.path, 'reports');
  const canReport = ['darwin', 'linux', 'win32'].includes(process.platform);
  if (canReport) fs.mkdirSync(reportRoot, { recursive: true });
  for (const [index, group] of atob.tests.entries()) {
    const serial = `group-probe-${process.pid}-${index}`;
    const reportPath = path.join(reportRoot, `report-html-webappapis-atob-${serial}.json`);
    try {
      const stdout = invoke(canReport ? __filename : driver('atob'),
                            { mode: 'run', source: group.source, key: group.key, variant: group.variant }, canReport ? {
                              WPT_REPORT: '1', TEST_SERIAL_ID: serial, NODE_TEST_WPT_REPORT_DIR: reportRoot,
                            } : {});
      const results = stdout.split('\n').filter((line) => line.startsWith('[PASS]'));
      assert.ok(results.length > 0);
      assert.ok(results.every((line) => line.includes(`${group.id}:`)));
      if (canReport) {
        const report = JSON.parse(fs.readFileSync(reportPath, 'utf8'));
        assert.deepStrictEqual(report.results.map((result) => result.test),
                               [`/html/webappapis/atob/${group.id}`]);
      }
    } finally {
      fs.rmSync(reportPath, { force: true });
    }
  }

  const encoding = discover('encoding');
  const queryGroups = encoding.tests.filter((test) => test.source === 'api-invalid-label.any.js');
  assert.strictEqual(queryGroups.length, 8);
  assert.deepStrictEqual([...new Set(queryGroups.map((test) => test.key))],
                         ['api-invalid-label.any.html', 'api-invalid-label.any.worker.html']);
  const timers = discover('timers');
  assert.strictEqual(timers.serial, true);
  const skipped = timers.tests.find((test) => test.source === 'negative-settimeout.any.js');
  assert.ok(skipped);
  const skippedOutput = invoke(driver('timers'), { mode: 'run', source: skipped.source, key: skipped.key });
  assert.match(skippedOutput, /\[SKIPPED\].*unreliable in Node\.js/);
  assert.match(skippedOutput, /1\.\.0 # SKIP/);
  assert.doesNotMatch(skippedOutput, /\[PASS\]/);

  if (common.hasSQLite) {
    const root = path.join(tmpdir.path, 'discovery');
    const directory = path.join(root, '.tmp.probe');
    fs.mkdirSync(directory, { recursive: true });
    const sentinel = path.join(directory, 'sentinel');
    fs.writeFileSync(sentinel, 'preserved');
    assert.strictEqual(discover('webstorage', { NODE_TEST_DIR: root, TEST_SERIAL_ID: 'probe' }).serial, true);
    assert.strictEqual(fs.readFileSync(sentinel, 'utf8'), 'preserved');
  }

  const config = { mode: 'run', source: 'compression-bad-chunks.any.js', key: 'compression-bad-chunks.any.html' };
  for (const probe of ['combined', 'mixed']) {
    const strict = discover('compression', { NODE_TEST_WPT_QUERY_PROBE: probe }, __filename);
    assert.deepStrictEqual(strict.tests, [{
      source: config.source, key: config.key, id: config.key, selector: `compression/${config.key}`,
    }]);
  }
  const flaky = discover('compression', { NODE_TEST_WPT_QUERY_PROBE: 'flaky' }, __filename);
  assert.deepStrictEqual(flaky.tests.map((test) => test.variant).sort(), ['?fail', '?pass']);
  assert.deepStrictEqual(flaky.tests.map((test) => test.id).sort(),
                         [`${config.key}?fail`, `${config.key}?pass`]);
  const single = invoke(__filename, { ...config, variant: '?pass' }, { NODE_TEST_WPT_QUERY_PROBE: 'flaky' });
  assert.match(single, /\.any\.html\?pass:/);
  assert.doesNotMatch(single, /\.any\.html\?fail:/);
  const combined = invoke(__filename, config, { NODE_TEST_WPT_QUERY_PROBE: 'combined' });
  assert.match(combined, /\.any\.html\?pass:/);
  assert.match(combined, /\.any\.html\?fail:/);
  const missing = invoke(__filename, config, { NODE_TEST_WPT_QUERY_PROBE: 'missing' }, 1);
  assert.match(missing, /Found 2 unexpected passes/);

  for (const query of ['', '?fail']) {
    const direct = spawnSync(process.execPath, [__filename, `compression/${config.key}${query}`], {
      env: { ...env, NODE_TEST_WPT_QUERY_PROBE: 'combined' },
      encoding: 'utf8', timeout: common.platformTimeout(10_000),
    });
    assert.ifError(direct.error);
    assert.strictEqual(direct.status, 0, direct.stdout + direct.stderr);
    assert.match(direct.stdout, /\.any\.html\?fail:/);
    if (query) assert.doesNotMatch(direct.stdout, /\.any\.html\?pass:/);
    else assert.match(direct.stdout, /\.any\.html\?pass:/);
  }
}
