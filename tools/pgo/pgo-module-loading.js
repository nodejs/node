'use strict';

/* eslint-disable no-void */

// PGO Training Script: Module Loading and Startup
//
// Module loading (require/import) is one of Node.js's most performance-critical
// paths, especially for:
// - Application startup time (serverless cold starts, CLI tools)
// - require() resolution algorithm (stat cascade, package.json parsing)
// - ESM vs CJS loading (import() dynamic imports)
// - Circular dependency resolution
// - Built-in module loading
// - JSON module loading (package.json, config files)
//
// This exercises: module resolver (fs.stat/readFile cascade), V8 script
// compilation, source code parsing, JSON parsing, path resolution,
// module wrapper function, exports/require machinery.

const path = require('path');
const fs = require('fs');
const os = require('os');
const vm = require('vm');
const Module = require('module');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;
const TEMP_DIR = path.join(
  os.tmpdir(),
  `node-pgo-module-${process.pid}-${Date.now()}`,
);

function setup() {
  fs.mkdirSync(TEMP_DIR, { recursive: true });

  // Create a realistic module tree
  // node_modules/
  //   express/
  //     package.json
  //     index.js
  //     lib/router.js, lib/request.js, lib/response.js
  //   lodash/
  //     package.json
  //     lodash.js
  //   config/
  //     package.json
  //     index.js
  //  src/
  //    index.js, utils.js, helpers.js, constants.js
  //    models/user.js, models/product.js
  //    middleware/auth.js, middleware/logger.js

  const dirs = [
    'node_modules/express/lib',
    'node_modules/lodash',
    'node_modules/config',
    'node_modules/debug',
    'node_modules/body-parser',
    'src/models',
    'src/middleware',
    'src/routes',
    'src/utils',
    'lib',
  ];

  for (const dir of dirs) {
    fs.mkdirSync(path.join(TEMP_DIR, dir), { recursive: true });
  }

  // Express-like module
  writeFile(
    'node_modules/express/package.json',
    JSON.stringify({
      name: 'express',
      version: '4.18.2',
      main: 'index.js',
    }),
  );
  writeFile(
    'node_modules/express/index.js',
    `
    'use strict';
    const router = require('./lib/router');
    const request = require('./lib/request');
    const response = require('./lib/response');
    function createApp() {
      const app = { routes: [], use(fn) { this.routes.push(fn); return this; } };
      Object.assign(app, router, request, response);
      return app;
    }
    module.exports = createApp;
    module.exports.Router = router.Router;
    module.exports.static = function(root) { return function(req, res, next) { next(); }; };
  `,
  );
  writeFile(
    'node_modules/express/lib/router.js',
    `
    'use strict';
    class Router { constructor() { this.stack = []; } route(path) { return this; } }
    exports.Router = function() { return new Router(); };
    exports.handle = function(req, res) {};
    exports.param = function(name, fn) {};
  `,
  );
  writeFile(
    'node_modules/express/lib/request.js',
    `
    'use strict';
    exports.get = function(field) { return ''; };
    exports.accepts = function() { return true; };
    exports.is = function(type) { return type; };
  `,
  );
  writeFile(
    'node_modules/express/lib/response.js',
    `
    'use strict';
    exports.send = function(body) { return this; };
    exports.json = function(obj) { return this; };
    exports.status = function(code) { return this; };
  `,
  );

  // Lodash-like module
  writeFile(
    'node_modules/lodash/package.json',
    JSON.stringify({
      name: 'lodash',
      version: '4.17.21',
      main: 'lodash.js',
    }),
  );
  writeFile(
    'node_modules/lodash/lodash.js',
    `
    'use strict';
    const _ = {};
    _.map = (arr, fn) => arr.map(fn);
    _.filter = (arr, fn) => arr.filter(fn);
    _.reduce = (arr, fn, init) => arr.reduce(fn, init);
    _.get = (obj, path, def) => { const keys = path.split('.'); let val = obj; for (const k of keys) { val = val?.[k]; } return val ?? def; };
    _.set = (obj, path, val) => { const keys = path.split('.'); let cur = obj; for (let i = 0; i < keys.length - 1; i++) { cur = cur[keys[i]] ??= {}; } cur[keys[keys.length-1]] = val; return obj; };
    _.cloneDeep = (obj) => JSON.parse(JSON.stringify(obj));
    _.debounce = (fn, wait) => { let timer; return (...args) => { clearTimeout(timer); timer = setTimeout(() => fn(...args), wait); }; };
    _.chunk = (arr, size) => { const res = []; for (let i = 0; i < arr.length; i += size) res.push(arr.slice(i, i + size)); return res; };
    _.flatten = (arr) => arr.flat();
    _.uniq = (arr) => [...new Set(arr)];
    module.exports = _;
  `,
  );

  // Config module
  writeFile(
    'node_modules/config/package.json',
    JSON.stringify({
      name: 'config',
      version: '3.3.9',
      main: 'index.js',
    }),
  );
  writeFile(
    'node_modules/config/index.js',
    `
    'use strict';
    const config = { db: { host: 'localhost', port: 5432 }, app: { port: 3000 } };
    module.exports = { get(key) { return key.split('.').reduce((o, k) => o?.[k], config); }, has(key) { return this.get(key) !== undefined; } };
  `,
  );

  // Debug module
  writeFile(
    'node_modules/debug/package.json',
    JSON.stringify({
      name: 'debug',
      version: '4.3.4',
      main: 'index.js',
    }),
  );
  writeFile(
    'node_modules/debug/index.js',
    `
    'use strict';
    module.exports = function(namespace) {
      return function(...args) { /* noop in production */ };
    };
  `,
  );

  // Body-parser module
  writeFile(
    'node_modules/body-parser/package.json',
    JSON.stringify({
      name: 'body-parser',
      version: '1.20.2',
      main: 'index.js',
    }),
  );
  writeFile(
    'node_modules/body-parser/index.js',
    `
    'use strict';
    exports.json = function(options) { return function(req, res, next) { next(); }; };
    exports.urlencoded = function(options) { return function(req, res, next) { next(); }; };
    exports.raw = function(options) { return function(req, res, next) { next(); }; };
  `,
  );

  // Application source files
  writeFile(
    'src/index.js',
    `
    'use strict';
    const express = require('express');
    const bodyParser = require('body-parser');
    const debug = require('debug');
    const config = require('config');
    const { authenticate } = require('./middleware/auth');
    const { logger } = require('./middleware/logger');
    const userRoutes = require('./routes/users');
    const { formatDate, generateId } = require('./utils/helpers');
    const { ROLES, STATUS } = require('./utils/constants');
    module.exports = { express, bodyParser, debug, config, authenticate, logger, userRoutes, formatDate, generateId, ROLES, STATUS };
  `,
  );

  writeFile(
    'src/utils/helpers.js',
    `
    'use strict';
    const crypto = require('crypto');
    exports.formatDate = (d) => new Date(d).toISOString();
    exports.generateId = () => crypto.randomUUID();
    exports.sanitize = (str) => str.replace(/[<>&"']/g, '');
    exports.paginate = (arr, page, size) => arr.slice((page - 1) * size, page * size);
  `,
  );

  writeFile(
    'src/utils/constants.js',
    `
    'use strict';
    exports.ROLES = Object.freeze({ ADMIN: 'admin', USER: 'user', GUEST: 'guest' });
    exports.STATUS = Object.freeze({ ACTIVE: 'active', INACTIVE: 'inactive', PENDING: 'pending' });
    exports.LIMITS = Object.freeze({ MAX_PAGE_SIZE: 100, MAX_UPLOAD: 10 * 1024 * 1024 });
  `,
  );

  writeFile(
    'src/models/user.js',
    `
    'use strict';
    const { generateId } = require('../utils/helpers');
    const { ROLES, STATUS } = require('../utils/constants');
    class User { constructor(data) { this.id = generateId(); this.role = ROLES.USER; this.status = STATUS.PENDING; Object.assign(this, data); } toJSON() { return { id: this.id, name: this.name, role: this.role }; } }
    module.exports = User;
  `,
  );

  writeFile(
    'src/models/product.js',
    `
    'use strict';
    const { generateId } = require('../utils/helpers');
    class Product { constructor(data) { this.id = generateId(); Object.assign(this, data); } toJSON() { return { id: this.id, name: this.name, price: this.price }; } }
    module.exports = Product;
  `,
  );

  writeFile(
    'src/middleware/auth.js',
    `
    'use strict';
    const crypto = require('crypto');
    exports.authenticate = function(req, res, next) { const token = req?.headers?.authorization; return !!token; };
    exports.authorize = function(...roles) { return function(req, res, next) { return roles.length > 0; }; };
  `,
  );

  writeFile(
    'src/middleware/logger.js',
    `
    'use strict';
    exports.logger = function(req, res, next) {
      const start = Date.now();
      return { duration: Date.now() - start, method: req?.method, url: req?.url };
    };
  `,
  );

  writeFile(
    'src/routes/users.js',
    `
    'use strict';
    const User = require('../models/user');
    const { paginate } = require('../utils/helpers');
    const { authenticate } = require('../middleware/auth');
    exports.getUsers = function(page, size) { return paginate([], page, size); };
    exports.createUser = function(data) { return new User(data); };
    exports.getUser = function(id) { return null; };
  `,
  );

  // ESM modules
  writeFile(
    'src/esm-entry.mjs',
    `
    import { createRequire } from 'module';
    import { fileURLToPath } from 'url';
    import { dirname, join } from 'path';
    const __filename = fileURLToPath(import.meta.url);
    const __dirname = dirname(__filename);
    const require = createRequire(import.meta.url);
    export const config = { loaded: true, dir: __dirname };
    export function greet(name) { return \`Hello, \${name}!\`; }
    export default { config, greet };
  `,
  );

  // JSON file (loaded as module)
  writeFile(
    'config/default.json',
    JSON.stringify(
      {
        server: { port: 3000, host: '0.0.0.0' },
        database: {
          url: 'postgres://localhost:5432/myapp',
          pool: { min: 2, max: 10 },
        },
        redis: { url: 'redis://localhost:6379' },
        jwt: { secret: 'test-secret', expiresIn: '1h' },
        logging: { level: 'info' },
      },
      null,
      2,
    ),
  );

  // package.json at root
  writeFile(
    'package.json',
    JSON.stringify(
      {
        name: 'pgo-test-app',
        version: '1.0.0',
        main: 'src/index.js',
        dependencies: {
          'express': '^4.18.0',
          'lodash': '^4.17.0',
          'config': '^3.3.0',
          'debug': '^4.3.0',
          'body-parser': '^1.20.0',
        },
      },
      null,
      2,
    ),
  );
}

function writeFile(relPath, content) {
  const fullPath = path.join(TEMP_DIR, relPath);
  fs.mkdirSync(path.dirname(fullPath), { recursive: true });
  fs.writeFileSync(fullPath, content);
}

function cleanup() {
  try {
    fs.rmSync(TEMP_DIR, { recursive: true, force: true });
  } catch {
    // best effort
  }
}

// Workload 1: require() with full resolution (CJS — the dominant pattern)
function workloadCJSRequire(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Clear module cache to force re-resolution
    const keys = Object.keys(require.cache).filter((k) =>
      k.startsWith(TEMP_DIR),
    );
    for (const key of keys) {
      delete require.cache[key];
    }

    // Require the full app (cascading dependencies)
    require(path.join(TEMP_DIR, 'src', 'index.js'));
    ops++;

    // Require individual modules
    require(path.join(TEMP_DIR, 'node_modules', 'lodash', 'lodash.js'));
    require(path.join(TEMP_DIR, 'src', 'models', 'user.js'));
    require(path.join(TEMP_DIR, 'src', 'models', 'product.js'));
    ops += 3;

    // JSON require (package.json resolution)
    delete require.cache[path.join(TEMP_DIR, 'package.json')];
    require(path.join(TEMP_DIR, 'package.json'));
    delete require.cache[path.join(TEMP_DIR, 'config', 'default.json')];
    require(path.join(TEMP_DIR, 'config', 'default.json'));
    ops += 2;
  }
  return ops;
}

// Workload 2: Built-in module loading (no resolution needed, but exercises native binding)
function workloadBuiltinRequire(iterations) {
  let ops = 0;
  const builtins = [
    'fs',
    'path',
    'http',
    'https',
    'crypto',
    'os',
    'url',
    'util',
    'events',
    'stream',
    'buffer',
    'net',
    'dns',
    'zlib',
    'child_process',
    'querystring',
    'string_decoder',
    'timers',
    'assert',
    'tls',
    'fs/promises',
    'stream/promises',
    'timers/promises',
    'node:fs',
    'node:path',
    'node:http',
    'node:crypto',
    'node:os',
  ];

  for (let i = 0; i < iterations; i++) {
    for (const mod of builtins) {
      require(mod);
    }
    ops += builtins.length;
  }
  return ops;
}

// Workload 3: Module.createRequire and resolution
function workloadModuleResolution(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // createRequire (used in ESM ↔ CJS interop)
    const customRequire = Module.createRequire(
      path.join(TEMP_DIR, 'src', 'index.js'),
    );

    // Resolve module paths (this is the hot path in require())
    try {
      customRequire.resolve('express');
    } catch {
      // expected
    }
    try {
      customRequire.resolve('lodash');
    } catch {
      // expected
    }
    try {
      customRequire.resolve('./models/user');
    } catch {
      // expected
    }
    try {
      customRequire.resolve('../package.json');
    } catch {
      // expected
    }
    ops += 4;

    // Module._resolveFilename (internal but exercises the same path)
    try {
      Module._resolveFilename('fs');
    } catch {
      // expected
    }
    try {
      Module._resolveFilename('path');
    } catch {
      // expected
    }
    ops += 2;

    // Module.builtinModules
    void Module.builtinModules;
    ops++;
  }
  return ops;
}

// Workload 4: vm.Script compilation (V8 script compilation path)
function workloadVMCompilation(iterations) {
  let ops = 0;

  const scripts = [
    // Simple expression
    'const x = 1 + 2; x;',
    // Function definition
    'function add(a, b) { return a + b; } add(1, 2);',
    // Object manipulation
    'const obj = { a: 1, b: { c: [1, 2, 3] } }; JSON.stringify(obj);',
    // Loop
    'let sum = 0; for (let i = 0; i < 1000; i++) sum += i; sum;',
    // Async-like pattern
    'const p = Promise.resolve(42); p.then(v => v * 2);',
    // Class definition (modern JS)
    `class Foo { constructor(x) { this.x = x; } get value() { return this.x; } }
     const f = new Foo(42); f.value;`,
    // Destructuring + spread
    'const { a, b, ...rest } = { a: 1, b: 2, c: 3, d: 4 }; [a, b, ...Object.values(rest)];',
    // Map/Set
    'const m = new Map([[1,"a"],[2,"b"]]); const s = new Set([1,2,3]); m.size + s.size;',
    // Regex
    'const re = /^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$/; re.test("test@example.com");',
    // Template literals
    // eslint-disable-next-line no-template-curly-in-string
    'const name = "World"; const greeting = `Hello, ${name}! Today is ${new Date().toISOString()}`; greeting;',
  ];

  for (let i = 0; i < iterations; i++) {
    for (const code of scripts) {
      const script = new vm.Script(code, {
        filename: `script-${i}.js`,
        produceCachedData: i % 5 === 0, // Some with code caching
      });

      const context = vm.createContext({
        JSON,
        Date,
        Promise,
        Map,
        Set,
        console,
      });
      script.runInContext(context, { timeout: 1000 });
      ops++;
    }
  }
  return ops;
}

// Workload 5: Dynamic import() (ESM loading — growing usage)
async function workloadDynamicImport(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Dynamic import of built-in modules (common in ESM code)
    await import('node:fs');
    await import('node:path');
    await import('node:crypto');
    await import('node:os');
    await import('node:util');
    await import('node:url');
    await import('node:events');
    await import('node:stream');
    ops += 8;
  }
  return ops;
}

// Workload 6: Module._compile simulation (exercises V8 compilation)
function workloadCompilePatterns(iterations) {
  let ops = 0;

  // Source code patterns that exercise different V8 compilation paths
  const sourcePatterns = [
    // Arrow functions (very common in modern JS)
    'module.exports = {\n' +
      Array.from(
        { length: 20 },
        (_, i) => `  fn${i}: (x) => x * ${i + 1},`,
      ).join('\n') +
      '\n};',

    // async/await (extremely common in server code)
    `module.exports = async function(data) {
      const result = await Promise.resolve(data);
      const items = await Promise.all(
        Array.from({length: 10}, (_, i) => Promise.resolve(i * 2))
      );
      return { result, items, total: items.reduce((a,b) => a+b, 0) };
    };`,

    // try/catch with specific error types
    `module.exports = function(input) {
      try {
        const data = JSON.parse(input);
        if (!data.id) throw new TypeError('Missing id');
        if (data.age < 0) throw new RangeError('Invalid age');
        return data;
      } catch (err) {
        if (err instanceof SyntaxError) return { error: 'Invalid JSON' };
        if (err instanceof TypeError) return { error: err.message };
        throw err;
      }
    };`,

    // Generator function (used in Koa, some ORMs)
    `module.exports = function* paginate(items, pageSize) {
      for (let i = 0; i < items.length; i += pageSize) {
        yield items.slice(i, i + pageSize);
      }
    };`,

    // Proxy/Reflect (used in Vue.js reactivity, ORMs)
    `module.exports = function createReactive(target) {
      return new Proxy(target, {
        get(obj, prop) { return Reflect.get(obj, prop); },
        set(obj, prop, value) { return Reflect.set(obj, prop, value); },
        has(obj, prop) { return Reflect.has(obj, prop); },
      });
    };`,
  ];

  for (let i = 0; i < iterations; i++) {
    for (const source of sourcePatterns) {
      const script = new vm.Script(
        `(function(exports, require, module, __filename, __dirname) { ${source} })`,
        { filename: `compile-${i}.js` },
      );
      const context = vm.createContext({
        JSON,
        Promise,
        Array,
        Object,
        Map,
        Set,
        Proxy,
        Reflect,
        TypeError,
        RangeError,
        SyntaxError,
        Error,
      });
      script.runInContext(context);
      ops++;
    }
  }
  return ops;
}

async function main() {
  console.log('[pgo-module-loading] Starting module loading workload...');

  setup();
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;

  const remaining = () => DURATION_MS - (Date.now() - startTime);

  try {
    while (remaining() > 0) {
      round++;
      const scale = Math.max(0.1, remaining() / DURATION_MS);
      const iterScale = (base) => Math.max(1, Math.floor(base * scale));

      // CJS require (the most common module operation)
      if (round === 1)
        console.log('[pgo-module-loading] Running CJS require...');
      totalOps += workloadCJSRequire(iterScale(100));
      if (remaining() <= 0) break;

      // Built-in module loading
      if (round === 1)
        console.log('[pgo-module-loading] Running built-in require...');
      totalOps += workloadBuiltinRequire(iterScale(200));
      if (remaining() <= 0) break;

      // Module resolution
      if (round === 1)
        console.log('[pgo-module-loading] Running module resolution...');
      totalOps += workloadModuleResolution(iterScale(200));
      if (remaining() <= 0) break;

      // VM compilation
      if (round === 1)
        console.log('[pgo-module-loading] Running VM compilation...');
      totalOps += workloadVMCompilation(iterScale(50));
      if (remaining() <= 0) break;

      // Compilation patterns
      if (round === 1)
        console.log('[pgo-module-loading] Running compile patterns...');
      totalOps += workloadCompilePatterns(iterScale(100));
      if (remaining() <= 0) break;

      // Dynamic import
      if (round === 1)
        console.log('[pgo-module-loading] Running dynamic import...');
      totalOps += await workloadDynamicImport(iterScale(50));
    }

    const elapsed = (Date.now() - startTime) / 1000;
    console.log(
      `[pgo-module-loading] Completed ${totalOps} ops in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
    );
  } finally {
    cleanup();
  }
}

main().catch((err) => {
  console.error('[pgo-module-loading] Error:', err);
  cleanup();
  process.exit(1);
});
