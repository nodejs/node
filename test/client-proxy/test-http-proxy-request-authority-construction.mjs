// Verify that the request path and Host header are constructed correctly
// for proxied requests across different option combinations.

import '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';

function makeAgent() {
  return new http.Agent({
    proxyEnv: { HTTP_PROXY: 'http://localhost:1' },
  });
}

function check(options, expectedPath, expectedHost) {
  const agent = makeAgent();
  const req = http.request({ agent, ...options });
  req.on('error', () => {});
  assert.strictEqual(req.path, expectedPath, `path for ${JSON.stringify(options)}`);
  assert.strictEqual(req.getHeader('host'), expectedHost, `Host for ${JSON.stringify(options)}`);
  req.destroy();
  agent.destroy();
}

const cases = [
  // [options, expectedPath, expectedHost]

  // OPTIONS * and CONNECT bypass path rewriting.
  [{ host: 'localhost', port: 3000, method: 'OPTIONS', path: '*' },
   '*', 'localhost:3000'],
  [{ host: 'example.com', port: 443, method: 'CONNECT', path: 'example.com:443' },
   'example.com:443', 'example.com:443'],

  // Basic cases: implicit Host, various port/defaultPort combos.
  [{ host: 'localhost', port: 3000, path: '/a' },
   'http://localhost:3000/a', 'localhost:3000'],
  [{ host: 'localhost', port: 80, path: '/b' },
   'http://localhost/b', 'localhost'],
  [{ host: 'example.com', path: '/c' },
   'http://example.com/c', 'example.com'],
  [{ host: 'localhost', port: '3000', path: '/d' },
   'http://localhost:3000/d', 'localhost:3000'],

  // defaultPort suppresses port in auto-set Host, canonicalization corrects it.
  [{ host: 'localhost', port: 80, defaultPort: 8080, path: '/e' },
   'http://localhost/e', 'localhost'],
  [{ host: 'localhost', port: 3000, defaultPort: 3000, path: '/f' },
   'http://localhost:3000/f', 'localhost:3000'],

  // User-explicit Host header: canonicalized to match request target.
  [{ host: 'localhost', port: 80, defaultPort: 8080, path: '/g', headers: { Host: 'localhost:80' } },
   'http://localhost/g', 'localhost'],
  [{ host: 'localhost', port: 80, defaultPort: 8080, path: '/h', headers: { Host: 'localhost' } },
   'http://localhost/h', 'localhost'],
  [{ host: 'localhost', port: 80, path: '/i', headers: { Host: 'localhost:80' } },
   'http://localhost/i', 'localhost'],
  [{ host: 'localhost', port: 3000, defaultPort: 3000, path: '/j', headers: { Host: 'localhost:3000' } },
   'http://localhost:3000/j', 'localhost:3000'],
  [{ host: 'localhost', port: 3000, path: '/k', headers: { Host: 'LOCALHOST:3000' } },
   'http://localhost:3000/k', 'localhost:3000'],

  // setHost=false with user-provided Host.
  [{ host: 'localhost', port: 3000, setHost: false, path: '/l', headers: { Host: 'localhost:3000' } },
   'http://localhost:3000/l', 'localhost:3000'],

  // IPv6.
  [{ host: '::1', port: 3000, path: '/m' },
   'http://[::1]:3000/m', '[::1]:3000'],
  [{ host: '::1', port: 80, path: '/n' },
   'http://[::1]/n', '[::1]'],

  // Mixed-case host option: URL normalizes to lowercase.
  [{ host: 'LocalHost', port: 3000, path: '/o' },
   'http://localhost:3000/o', 'localhost:3000'],

  // Absolute-form path matching authority.
  [{ host: 'localhost', port: 3000, path: 'http://localhost:3000/p', headers: { Host: 'localhost:3000' } },
   'http://localhost:3000/p', 'localhost:3000'],
];

for (const [options, expectedPath, expectedHost] of cases) {
  check(options, expectedPath, expectedHost);
}
