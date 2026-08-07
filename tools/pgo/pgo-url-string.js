'use strict';

/* eslint-disable no-void */

// PGO Training Script: URL Parsing and String Operations
//
// URL parsing and string manipulation are pervasive in Node.js web apps:
// - WHATWG URL parsing (every HTTP request in modern code)
// - Legacy url.parse (still heavily used in existing codebases)
// - URLSearchParams (query string handling in every API)
// - Regex matching (routing, validation, sanitization)
// - String operations (template rendering, concatenation, encoding)
// - TextEncoder/TextDecoder (Web API string encoding)
// - querystring module (legacy but still widely used)
// - util.format, util.inspect (logging, debugging)
//
// This exercises: V8 string internals (one-byte/two-byte, cons, sliced),
// Ada URL parser (C++), regex JIT, TextEncoder/Decoder C++ implementation.

const url = require('url');
const { URL, URLSearchParams } = url;
const querystring = require('querystring');
const util = require('util');
const { TextEncoder, TextDecoder } = util;

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;

// Realistic URLs from web applications
const TEST_URLS = [
  'https://api.example.com/v2/users?page=1&limit=50&sort=name&order=asc',
  'https://www.example.com/products/category/electronics?brand=apple&min_price=100&max_price=2000&in_stock=true',
  'http://localhost:3000/api/auth/login',
  'https://cdn.example.com/assets/images/hero-banner-2024.webp?w=1920&h=1080&q=85',
  'https://user:pass@db.example.com:5432/myapp?ssl=true&pool=10',
  'wss://realtime.example.com/ws/v1?token=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0',
  'https://example.com/path/to/resource#section-2',
  'https://search.example.com/q?query=node.js+performance+optimization&lang=en&safe=on',
  'https://api.example.com/v3/organizations/org-123/projects/proj-456/deployments/deploy-789/logs?since=2024-01-01T00:00:00Z&until=2024-12-31T23:59:59Z&level=error',
  'https://example.com/path%20with%20spaces/file%23name.html?key=value%26more',
  'https://xn--nxasmq6b.example.com/internationalized', // IDN
  'file:///C:/Users/user/Documents/project/index.html',
  'https://api.github.com/repos/nodejs/node/commits?sha=main&per_page=30',
  'https://registry.npmjs.org/@types/node/-/node-20.10.0.tgz',
  'http://[::1]:8080/ipv6-local',
  'data:text/html;base64,PGh0bWw+PC9odG1sPg==',
];

// Workload 1: WHATWG URL parsing (modern standard — every HTTP request)
function workloadWHATWGURL(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    for (const urlStr of TEST_URLS) {
      try {
        const parsed = new URL(urlStr);

        // Access all properties (common in routing/logging)
        void parsed.href;
        void parsed.origin;
        void parsed.protocol;
        void parsed.hostname;
        void parsed.port;
        void parsed.pathname;
        void parsed.search;
        void parsed.hash;
        void parsed.username;
        void parsed.password;
        ops++;

        // Modify URL (redirect, base URL construction)
        const modified = new URL(urlStr);
        modified.pathname = '/new-path';
        modified.searchParams.set('modified', 'true');
        modified.toString();
        ops++;

        // URL.canParse (validation)
        URL.canParse(urlStr);
        URL.canParse('not-a-url');
        ops++;
      } catch {
        ops++;
      }
    }
  }
  return ops;
}

// Workload 2: URLSearchParams (query string handling)
function workloadURLSearchParams(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Create from string
    const params1 = new URLSearchParams(
      'page=1&limit=50&sort=name&order=asc&fields=id,name,email',
    );
    params1.get('page');
    params1.get('limit');
    params1.getAll('fields');
    params1.has('sort');
    ops++;

    // Create from object (common API pattern)
    const params2 = new URLSearchParams({
      q: 'node.js performance',
      lang: 'en',
      page: '1',
      format: 'json',
    });
    params2.toString();
    ops++;

    // Create from entries (Map-like)
    const params3 = new URLSearchParams([
      ['key1', 'value1'],
      ['key2', 'value2'],
      ['key1', 'value3'],
    ]);
    params3.getAll('key1');
    ops++;

    // Iterate (common for logging, forwarding)
    for (const entry of params1) {
      void entry;
    }
    ops++;

    // Modify (adding/removing filters in API clients)
    params1.set('page', '2');
    params1.append('filter', 'active');
    params1.append('filter', 'verified');
    params1.delete('order');
    params1.sort();
    params1.toString();
    ops++;

    // Complex real-world query strings
    const complexQuery = new URLSearchParams(
      'filters[status]=active&filters[role][]=admin&filters[role][]=editor&' +
        'sort=-createdAt&fields=id,name,email,role&page[number]=1&page[size]=25&' +
        'include=profile,permissions&meta=true',
    );
    for (const entry of complexQuery) {
      void entry;
    }
    complexQuery.toString();
    ops++;
  }
  return ops;
}

// Workload 3: Legacy url.parse (still very common in existing code)
function workloadLegacyURL(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    for (const urlStr of TEST_URLS) {
      try {
        // url.parse (Express's req.url handling)
        const parsed = url.parse(urlStr, true); // true = parse query string
        void parsed.pathname;
        void parsed.query;
        void parsed.hostname;
        void parsed.protocol;
        ops++;

        // url.format (building URLs)
        url.format({
          protocol: 'https',
          hostname: 'api.example.com',
          pathname: `/v2/users/${i}`,
          query: { fields: 'id,name', format: 'json' },
        });
        ops++;

        // url.resolve (relative URL resolution)
        url.resolve('https://example.com/base/', './relative/path');
        url.resolve('https://example.com/base/path', '../other/path');
        ops++;
      } catch {
        ops++;
      }
    }
  }
  return ops;
}

// Workload 4: querystring module (legacy but still heavily used)
function workloadQuerystring(iterations) {
  let ops = 0;

  const testQueries = [
    'name=John+Doe&age=30&city=New+York',
    'key1=val1&key2=val2&key3=val3&key4=val4&key5=val5',
    'q=search+term&page=1&per_page=20&sort=relevance',
    'data=%7B%22key%22%3A%22value%22%7D',
    'tags=node&tags=javascript&tags=performance',
  ];

  for (let i = 0; i < iterations; i++) {
    for (const qs of testQueries) {
      // Parse
      const parsed = querystring.parse(qs);
      ops++;

      // Stringify
      querystring.stringify(parsed);
      ops++;
    }

    // Encode/decode
    querystring.escape('Hello World! <>&"');
    querystring.unescape('Hello%20World%21%20%3C%3E%26%22');
    ops += 2;

    // Stringify with custom separator (some APIs use ; instead of &)
    querystring.stringify({ a: 1, b: 2, c: 3 }, ';', ':');
    ops++;
  }
  return ops;
}

// Workload 5: String operations (templating, manipulation)
function workloadStringOps(iterations) {
  let ops = 0;

  const template = 'Hello, {{name}}! You have {{count}} new {{type}} messages.';
  const strings = [
    'the quick brown fox jumps over the lazy dog',
    'UPPERCASE STRING TO CONVERT',
    '  whitespace  string  with  extra  spaces  ',
    'CamelCaseString_with_mixed_SEPARATORS-and-dashes',
    'path/to/some/file.extension.backup.2024',
  ];

  for (let i = 0; i < iterations; i++) {
    // Template replacement (Mustache/Handlebars-like pattern)
    template.replace(/\{\{(\w+)\}\}/g, (match, key) => {
      const values = { name: 'User', count: '5', type: 'unread' };
      return values[key] || match;
    });
    ops++;

    for (const str of strings) {
      // Case conversion (header normalization, camelCase conversion)
      str.toLowerCase();
      str.toUpperCase();
      ops += 2;

      // Trim (input sanitization)
      str.trim();
      str.trimStart();
      str.trimEnd();
      ops += 3;

      // Split/join (CSV processing, path parsing)
      str.split(/[\s_-]+/).join(' ');
      str.split('.').join('/');
      ops += 2;

      // Include/startsWith/endsWith (routing, MIME detection)
      str.includes('fox');
      str.startsWith('the');
      str.endsWith('dog');
      str.indexOf('the');
      str.lastIndexOf('the');
      ops += 5;

      // Slice/substring (truncation, preview generation)
      str.slice(0, 20);
      str.substring(5, 15);
      ops += 2;

      // Replace (sanitization, normalization)
      str.replace(/[^a-zA-Z0-9]/g, '_');
      str.replaceAll(' ', '-');
      ops += 2;

      // padStart/padEnd (formatting)
      String(i).padStart(6, '0');
      str.slice(0, 10).padEnd(20, '.');
      ops += 2;

      // Repeat (indentation generation)
      '  '.repeat(Math.min(i % 10, 5));
      ops++;
    }

    // String concatenation patterns
    let result = '';
    for (let j = 0; j < 100; j++) {
      result += `item_${j},`;
    }
    ops++;
    void result;

    // Template literal (most common string building pattern)
    const items = Array.from(
      { length: 20 },
      (_, j) => `  "${j}": "value_${j}"`,
    );
    const json = `{\n${items.join(',\n')}\n}`;
    ops++;
    void json;

    // String.fromCharCode / codePointAt (encoding)
    for (let j = 32; j < 127; j++) {
      String.fromCharCode(j);
    }
    'Hello! 日本語'.codePointAt(0);
    ops++;
  }
  return ops;
}

// Workload 6: Regular expressions (routing, validation, parsing)
function workloadRegex(iterations) {
  let ops = 0;

  // Pre-compiled regexes (real-world patterns)
  const emailRe = /^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$/;
  const uuidRe =
    /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  const ipv4Re = /^(\d{1,3}\.){3}\d{1,3}$/;
  const routeRe =
    /^\/api\/v(\d+)\/(users|products|orders)(?:\/([a-zA-Z0-9_-]+))?$/;
  const htmlTagRe = /<([a-z]+)([^>]*)>(.*?)<\/\1>/gi;
  const semverRe =
    /^v?(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.]+))?(?:\+([a-zA-Z0-9.]+))?$/;
  const dateRe = /^\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2}:\d{2}(?:\.\d{3})?Z?)?$/;
  const csvRe = /(?:^|,)("(?:[^"]*(?:""[^"]*)*)"|[^,]*)/g;

  const testStrings = {
    emails: ['test@example.com', 'invalid@', 'user+tag@domain.co.uk', 'nope'],
    uuids: [
      '550e8400-e29b-41d4-a716-446655440000',
      'not-a-uuid',
      '550E8400-E29B-41D4-A716-446655440000',
    ],
    ips: ['192.168.1.1', '10.0.0.0', '999.999.999.999', 'not.an.ip'],
    routes: [
      '/api/v2/users',
      '/api/v1/products/abc-123',
      '/api/v3/orders/',
      '/other/path',
    ],
    html: [
      '<div class="container">Hello</div>',
      '<p>Text</p>',
      '<span id="x">Content</span>',
    ],
    semver: ['v1.2.3', '2.0.0-beta.1', '1.0.0+build.123', '1.2', 'not-semver'],
    dates: [
      '2024-01-15',
      '2024-01-15T10:30:00Z',
      '2024-01-15T10:30:00.123Z',
      'not-a-date',
    ],
    csv: [
      '"John","Doe",30',
      '"Jane ""J"" Doe","Smith",25',
      'simple,values,here',
    ],
  };

  for (let i = 0; i < iterations; i++) {
    // Email validation (form submission, user registration)
    for (const email of testStrings.emails) {
      emailRe.test(email);
      ops++;
    }

    // UUID validation (API parameters)
    for (const uuid of testStrings.uuids) {
      uuidRe.test(uuid);
      ops++;
    }

    // IP validation (rate limiting, geo-blocking)
    for (const ip of testStrings.ips) {
      ipv4Re.test(ip);
      ops++;
    }

    // Route matching (URL routing in Express/Fastify)
    for (const route of testStrings.routes) {
      const match = routeRe.exec(route);
      if (match) {
        void match[1];
        void match[2];
        void match[3];
      }
      ops++;
    }

    // HTML tag parsing (template engines, sanitization)
    for (const html of testStrings.html) {
      html.replace(htmlTagRe, (match, tag, attrs, content) => content);
      ops++;
    }

    // Semver parsing (package resolution)
    for (const ver of testStrings.semver) {
      semverRe.test(ver);
      ops++;
    }

    // Date validation (API input)
    for (const date of testStrings.dates) {
      dateRe.test(date);
      ops++;
    }

    // CSV parsing (data import)
    for (const csv of testStrings.csv) {
      void [...csv.matchAll(csvRe)];
      ops++;
    }
  }
  return ops;
}

// Workload 7: TextEncoder/TextDecoder (Web API compatibility)
function workloadTextEncoding(iterations) {
  let ops = 0;
  const encoder = new TextEncoder();
  const decoder = new TextDecoder('utf-8');
  const testStrings = [
    'Hello, World!',
    'The quick brown fox jumps over the lazy dog',
    '日本語テスト Unicode text with émojis 🎉',
    'A'.repeat(1000),
    Array.from({ length: 100 }, (_, i) => `line ${i}`).join('\n'),
  ];

  for (let i = 0; i < iterations; i++) {
    for (const str of testStrings) {
      // Encode (string → Uint8Array)
      const encoded = encoder.encode(str);
      ops++;

      // Decode (Uint8Array → string)
      decoder.decode(encoded);
      ops++;

      // encodeInto (reuse buffer — more efficient)
      const dest = new Uint8Array(str.length * 3);
      encoder.encodeInto(str, dest);
      ops++;
    }
  }
  return ops;
}

// Workload 8: util.format and util.inspect (logging — extremely common)
function workloadUtilFormat(iterations) {
  let ops = 0;

  const testObjects = [
    { simple: 'object', count: 42 },
    { nested: { deep: { value: [1, 2, 3] } }, fn: () => {} },
    new Map([['key', 'value']]),
    new Set([1, 2, 3]),
    new Error('test error'),
    Buffer.from('test'),
    /regex/gi,
    new Date(),
  ];

  for (let i = 0; i < iterations; i++) {
    // util.format (console.log uses this internally)
    util.format('Request %s %s completed in %dms', 'GET', '/api/users', 42.5);
    util.format('User %j logged in', { id: 1, name: 'test' });
    util.format('Values: %o %O', { a: 1 }, { b: 2 });
    ops += 3;

    // util.inspect (debug output, REPL)
    for (const obj of testObjects) {
      util.inspect(obj, { depth: 3, colors: false, maxArrayLength: 10 });
      ops++;
    }

    // util.types (type checking)
    util.types.isDate(new Date());
    util.types.isRegExp(/test/);
    util.types.isSet(new Set());
    util.types.isMap(new Map());
    util.types.isPromise(Promise.resolve());
    ops += 5;

    // util.deprecate (module system pattern)
    util.deprecate(() => {}, 'Use newAPI instead');
    ops++;
  }
  return ops;
}

async function main() {
  console.log('[pgo-url-string] Starting URL & string workload...');
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;

  const remaining = () => DURATION_MS - (Date.now() - startTime);

  while (remaining() > 0) {
    round++;
    const scale = Math.max(0.1, remaining() / DURATION_MS);
    const iterScale = (base) => Math.max(1, Math.floor(base * scale));

    // WHATWG URL (highest priority — modern web standard)
    if (round === 1)
      console.log('[pgo-url-string] Running WHATWG URL parsing...');
    totalOps += workloadWHATWGURL(iterScale(300));
    if (remaining() <= 0) break;

    // URLSearchParams
    if (round === 1) console.log('[pgo-url-string] Running URLSearchParams...');
    totalOps += workloadURLSearchParams(iterScale(500));
    if (remaining() <= 0) break;

    // Legacy URL
    if (round === 1)
      console.log('[pgo-url-string] Running legacy url.parse...');
    totalOps += workloadLegacyURL(iterScale(200));
    if (remaining() <= 0) break;

    // Querystring
    if (round === 1) console.log('[pgo-url-string] Running querystring...');
    totalOps += workloadQuerystring(iterScale(500));
    if (remaining() <= 0) break;

    // String operations
    if (round === 1)
      console.log('[pgo-url-string] Running string operations...');
    totalOps += workloadStringOps(iterScale(300));
    if (remaining() <= 0) break;

    // Regex
    if (round === 1) console.log('[pgo-url-string] Running regex patterns...');
    totalOps += workloadRegex(iterScale(500));
    if (remaining() <= 0) break;

    // TextEncoder/Decoder
    if (round === 1)
      console.log('[pgo-url-string] Running TextEncoder/Decoder...');
    totalOps += workloadTextEncoding(iterScale(500));
    if (remaining() <= 0) break;

    // util.format/inspect
    if (round === 1)
      console.log('[pgo-url-string] Running util.format/inspect...');
    totalOps += workloadUtilFormat(iterScale(200));
  }

  const elapsed = (Date.now() - startTime) / 1000;
  console.log(
    `[pgo-url-string] Completed ${totalOps} ops in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
  );
}

main().catch((err) => {
  console.error('[pgo-url-string] Error:', err);
  process.exit(1);
});
