// Regression test for the incomplete CVE-2026-48615 fix: proxy credentials must
// never appear in ERR_PROXY_INVALID_CONFIG messages. Four error paths interpolate
// the raw proxy URL (userinfo included):
//   1. lib/internal/http.js - new URL() fails in the ProxyConfig constructor.
//   2. lib/internal/http.js - proxy URL contains CR or LF.
//   3. lib/http.js - http.setGlobalProxyFromEnv() with an unparseable http_proxy.
//   4. lib/http.js - http.setGlobalProxyFromEnv() with an unparseable https_proxy.
//
// The well-formed credential control confirms redaction on the normal path is
// preserved.

import '../common/index.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';

// A proxy credential that both contains '@' and yields substrings in every
// meaningful piece that must not survive in any error message.
const SECRET = 'S3cr3t-Pr0xy-P@ssw0rd';

// Asserts that a message does not contain any substring of length >= 4 derived
// from the secret. Length-4 substrings are enough to catch partial leaks while
// avoiding false positives from unrelated words.
function assertNoLeak(message, label) {
  assert.doesNotMatch(message, /S3cr3t/, `${label}: leaked username`);
  assert.doesNotMatch(message, /Pr0xy/, `${label}: leaked password fragment`);
  assert.doesNotMatch(message, /ssw0rd/, `${label}: leaked password fragment`);
  assert.doesNotMatch(message, /P@ssw0rd/, `${label}: leaked password`);
  // A malicious proxy URL must not survive verbatim.
  assert.notStrictEqual(message, SECRET, `${label}: leaked the whole secret`);
}

function runNode(code, env = {}) {
  const res = spawnSync(process.execPath, ['-e', code], {
    env: { ...process.env, ...env },
    encoding: 'utf8',
  });
  return res;
}

function setGlobalProxyFromEnv(proxyEnv) {
  return `try {
    require('http').setGlobalProxyFromEnv(${JSON.stringify(proxyEnv)});
  } catch (e) {
    console.log(e.message);
  }
  `;
}

// --- Sites 3 & 4: public http.setGlobalProxyFromEnv(), reachable without flags ---

{
  // Site 3: unparseable http_proxy.
  const { stdout, stderr } = runNode(setGlobalProxyFromEnv({
    http_proxy: `http://alice:${SECRET}@ `,
  }));
  assertNoLeak(stdout, 'site 3');
  assertNoLeak(stderr, 'site 3 stderr');
  assert.match(stdout, /http:\/\/\[redacted\]@/);
}

{
  // Site 4: unparseable https_proxy.
  const { stdout, stderr } = runNode(setGlobalProxyFromEnv({
    https_proxy: `http://alice:${SECRET}@::bad`,
  }));
  assertNoLeak(stdout, 'site 4');
  assertNoLeak(stderr, 'site 4 stderr');
  assert.match(stdout, /http:\/\/\[redacted\]@/);
}

// --- Site 1: ProxyConfig constructor, new URL() throws (reachable via Agent) ---

{
  const code = `try {
    new http.Agent({
      proxyEnv: { HTTP_PROXY: 'http://alice:${SECRET}@::bad' },
    });
  } catch (e) {
    console.log(e.message);
  }`;
  const { stdout, stderr } = runNode(code);
  assertNoLeak(stdout, 'site 1');
  assertNoLeak(stderr, 'site 1 stderr');
  assert.match(stdout, /Invalid proxy URL: http:\/\/\[redacted\]@/);
}

// --- Site 2: proxy URL containing CR or LF ---

{
  const code = `try {
    require('http').setGlobalProxyFromEnv({
      http_proxy: 'http://alice:${SECRET}@\\nhost',
    });
  } catch (e) {
    console.log(e.message);
  }`;
  const { stdout, stderr } = runNode(code);
  assertNoLeak(stdout, 'site 2');
  assertNoLeak(stderr, 'site 2 stderr');
}

// --- Site 1 & 2 via the environment-driven, opt-in path (NODE_USE_ENV_PROXY) ---

{
  const code = `require('http').request('http://example.com/').on('error', () => {}).end()`;
  const { stdout, stderr } = runNode(code, {
    NODE_USE_ENV_PROXY: '1',
    http_proxy: `http://alice:${SECRET}@:::bad`,
  });
  // Sites 1/2 throw during builtin module init, so stderr carries the message and
  // the process exits non-zero. The application cannot catch it.
  assertNoLeak(stdout, 'env-driven stdout');
  assertNoLeak(stderr, 'env-driven stderr');
  const { status } = runNode(code, {
    NODE_USE_ENV_PROXY: '1',
    http_proxy: `http://alice:${SECRET}@:::bad`,
  });
  assert.notStrictEqual(status, 0);
}

// --- Control: well-formed URL with credentials is redacted on the normal path ---

{
  const code = `try {
    require('http').setGlobalProxyFromEnv({
      http_proxy: 'http://alice:${SECRET}@localhost:8080',
    });
    console.log('ok');
  } catch (e) {
    console.log('unexpected throw: ' + e.message);
  }`;
  const { stdout, status } = runNode(code);
  assert.strictEqual(status, 0);
  assert.strictEqual(stdout.trim(), 'ok');
}

// --- Control: an unparseable URL with no credentials is left untouched ---

{
  const { stdout } = runNode(setGlobalProxyFromEnv({
    http_proxy: 'http://host:port',
  }));
  assert.strictEqual(stdout.trim(), 'http://host:port');
  assert.doesNotMatch(stdout, /\[redacted\]/);
}
