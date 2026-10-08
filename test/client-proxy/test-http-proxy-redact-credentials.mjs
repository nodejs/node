// Regression test for the incomplete CVE-2026-48615 fix: proxy credentials must
// never appear in ERR_PROXY_INVALID_CONFIG messages. Two error paths interpolate
// the raw proxy URL (userinfo included):
//   1. lib/internal/http.js - new URL() fails in the ProxyConfig constructor.
//   2. lib/internal/http.js - proxy URL contains CR or LF.
//
// The well-formed credential control confirms redaction on the normal path is
// preserved.

import '../common/index.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';

// A proxy credential that both contains '@' and yields substrings in every
// meaningful piece that must not survive in any error message.
const SECRET = 'S3cr3t-Pr0xy-P@ssw0rd';

function assertNoLeak(message, label) {
  assert.doesNotMatch(message, /S3cr3t/, `${label}: leaked username`);
  assert.doesNotMatch(message, /Pr0xy/, `${label}: leaked password fragment`);
  assert.doesNotMatch(message, /ssw0rd/, `${label}: leaked password fragment`);
  assert.doesNotMatch(message, /P@ssw0rd/, `${label}: leaked password`);
}

function runNode(code, env = {}) {
  const res = spawnSync(process.execPath, ['-e', code], {
    env: { ...process.env, ...env },
    encoding: 'utf8',
  });
  return res;
}

function agentWithProxyEnv(proxyEnv) {
  return `try {
    new (require('http').Agent)({ proxyEnv: ${JSON.stringify(proxyEnv)} });
    console.log('ok');
  } catch (e) {
    console.log(e.message);
  }`;
}

// --- Site 1: ProxyConfig constructor, new URL() throws ---

{
  const { stdout, stderr } = runNode(agentWithProxyEnv({
    HTTP_PROXY: `http://alice:${SECRET}@::bad`,
  }));
  assertNoLeak(stdout, 'site 1');
  assertNoLeak(stderr, 'site 1 stderr');
  assert.match(stdout, /Invalid proxy URL: http:\/\/\[redacted\]@/);
}

// --- Site 2: proxy URL containing CR or LF ---

{
  const { stdout, stderr } = runNode(agentWithProxyEnv({
    HTTP_PROXY: `http://alice:${SECRET}@\nhost`,
  }));
  assertNoLeak(stdout, 'site 2');
  assertNoLeak(stderr, 'site 2 stderr');
  assert.match(stdout, /Invalid proxy URL: /);
}

// --- Sites 1 & 2 via the environment-driven, opt-in path (NODE_USE_ENV_PROXY) ---

{
  const code = `require('http').request('http://example.com/').on('error', () => {}).end()`;
  const { stdout, stderr, status } = runNode(code, {
    NODE_USE_ENV_PROXY: '1',
    http_proxy: `http://alice:${SECRET}@:::bad`,
  });
  // The error is thrown while the global agent is created, so stderr carries
  // the message and the process exits non-zero.
  assertNoLeak(stdout, 'env-driven stdout');
  assertNoLeak(stderr, 'env-driven stderr');
  assert.notStrictEqual(status, 0);
}

// --- Control: well-formed URL with credentials does not throw ---

{
  const { stdout, status } = runNode(agentWithProxyEnv({
    HTTP_PROXY: `http://alice:${SECRET}@localhost:8080`,
  }));
  assert.strictEqual(status, 0);
  assert.strictEqual(stdout.trim(), 'ok');
}

// --- Control: an unparseable URL with no credentials is left untouched ---

{
  const { stdout } = runNode(agentWithProxyEnv({
    HTTP_PROXY: 'http://host:port',
  }));
  assert.strictEqual(stdout.trim(), 'Invalid proxy URL: http://host:port');
  assert.doesNotMatch(stdout, /\[redacted\]/);
}
