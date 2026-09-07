'use strict';

const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const events = require('events');
const os = require('os');
const { inspect } = require('util');
const { pathToFileURL } = require('url');
const { Worker } = require('worker_threads');
const { fork } = require('child_process');

const workerPath = path.join(__dirname, 'wpt/worker.js');
const wptNonTestDirs = new Set(['resources', 'support', 'tools']);

function getBrowserProperties() {
  const { node: version } = process.versions; // e.g. 18.13.0, 20.0.0-nightly202302078e6e215481
  const release = /^\d+\.\d+\.\d+$/.test(version);
  const browser = {
    browser_channel: release ? 'stable' : 'experimental',
    browser_version: version,
  };

  return browser;
}

/**
 * Return one of three expected values
 * https://github.com/web-platform-tests/wpt/blob/1c6ff12/tools/wptrunner/wptrunner/tests/test_update.py#L953-L958
 * @returns {'linux'|'mac'|'win'}
 */
function getOs() {
  switch (os.type()) {
    case 'Linux':
      return 'linux';
    case 'Darwin':
      return 'mac';
    case 'Windows_NT':
      return 'win';
    default:
      throw new Error('Unsupported os.type()');
  }
}

// https://github.com/web-platform-tests/wpt/blob/b24eedd/resources/testharness.js#L3705
function sanitizeUnpairedSurrogates(str) {
  return str.replace(
    /([\ud800-\udbff]+)(?![\udc00-\udfff])|(^|[^\ud800-\udbff])([\udc00-\udfff]+)/g,
    function(_, low, prefix, high) {
      let output = prefix || '';  // Prefix may be undefined
      const string = low || high;  // Only one of these alternates can match
      for (let i = 0; i < string.length; i++) {
        output += codeUnitStr(string[i]);
      }
      return output;
    });
}

function codeUnitStr(char) {
  return 'U+' + char.charCodeAt(0).toString(16);
}

class ReportResult {
  #startTime;

  constructor(name) {
    this.test = name;
    this.status = 'OK';
    this.subtests = [];
    this.#startTime = Date.now();
  }

  addSubtest(name, status, message) {
    const subtest = {
      status,
      // https://github.com/web-platform-tests/wpt/blob/b24eedd/resources/testharness.js#L3722
      name: sanitizeUnpairedSurrogates(name),
    };
    if (message) {
      // https://github.com/web-platform-tests/wpt/blob/b24eedd/resources/testharness.js#L4506
      subtest.message = sanitizeUnpairedSurrogates(message);
    }
    this.subtests.push(subtest);
    return subtest;
  }

  finish(status) {
    this.status = status ?? 'OK';
    this.duration = Date.now() - this.#startTime;
  }
}

// Generates a report that can be uploaded to wpt.fyi.
// Checkout https://github.com/web-platform-tests/wpt.fyi/tree/main/api#results-creation
// for more details.
class WPTReport {
  constructor(testPath) {
    this.filename = `report-${testPath.replaceAll('/', '-')}.json`;
    this.filepath = path.join(__dirname, `../../out/wpt/${this.filename}`);
    /** @type {Map<string, ReportResult>} */
    this.results = new Map();
    this.time_start = Date.now();
  }

  /**
   * Get or create a ReportResult for a test spec.
   * @param {WPTTestSpec} spec
   * @returns {ReportResult}
   */
  getResult(spec) {
    const name = `/${spec.getTestPath()}`;
    if (this.results.has(name)) {
      return this.results.get(name);
    }
    const result = new ReportResult(name);
    this.results.set(name, result);
    return result;
  }

  /**
   * @returns {void}
   */
  write() {
    this.time_end = Date.now();
    const results = Array.from(this.results.values());

    /**
     * Return required and some optional properties
     * https://github.com/web-platform-tests/wpt.fyi/blob/60da175/api/README.md?plain=1#L331-L335
     */
    this.run_info = {
      product: 'node.js',
      ...getBrowserProperties(),
      revision: process.env.WPT_REVISION || 'unknown',
      os: getOs(),
    };

    fs.writeFileSync(this.filepath, JSON.stringify({
      time_start: this.time_start,
      time_end: this.time_end,
      run_info: this.run_info,
      results: results,
    }));
  }
}

// https://github.com/web-platform-tests/wpt/blob/HEAD/resources/testharness.js
// TODO: get rid of this half-baked harness in favor of the one
// pulled from WPT
const harnessMock = {
  test: (fn, desc) => {
    try {
      fn();
    } catch (err) {
      console.error(`In ${desc}:`);
      throw err;
    }
  },
  assert_equals: assert.strictEqual,
  assert_true: (value, message) => assert.strictEqual(value, true, message),
  assert_false: (value, message) => assert.strictEqual(value, false, message),
  assert_throws: (code, func, desc) => {
    assert.throws(func, function(err) {
      return typeof err === 'object' &&
             'name' in err &&
             err.name.startsWith(code.name);
    }, desc);
  },
  assert_array_equals: assert.deepStrictEqual,
  assert_unreached(desc) {
    assert.fail(`Reached unreachable code: ${desc}`);
  },
};

class ResourceLoader {
  constructor(path) {
    this.path = path;
  }

  toRealFilePath(from, url) {
    // We need to patch this to load the WebIDL parser
    url = url.replace(
      '/resources/WebIDLParser.js',
      '/resources/webidl2/lib/webidl2.js',
    );
    const base = path.dirname(from);
    return url.startsWith('/') ?
      fixtures.path('wpt', url) :
      fixtures.path('wpt', base, url);
  }

  /**
   * Map a URL that a test would have fetched from the WPT server (an
   * absolute path, or a path relative to the test file) to a file: URL
   * into the fixtures directory. URLs that already have a scheme (data:,
   * blob:, http:, ...) are returned unchanged.
   * @param {string} from the path of the file loading this resource,
   *   relative to the WPT folder.
   * @param {string|URL} url the url of the resource being loaded.
   * @returns {string}
   */
  mapServerURL(from, url) {
    url = `${url}`;
    if (/^[a-zA-Z][a-zA-Z0-9+.-]*:|^\/\//.test(url)) {
      return url;
    }
    return pathToFileURL(this.toRealFilePath(from, url)).href;
  }

  /**
   * Load a resource in test/fixtures/wpt specified with a URL
   * @param {string} from the path of the file loading this resource,
   *   relative to the WPT folder.
   * @param {string} url the url of the resource being loaded.
   * @returns {string}
   */
  read(from, url) {
    const file = this.toRealFilePath(from, url);
    return fs.readFileSync(file, 'utf8');
  }

  /**
   * Load a resource in test/fixtures/wpt specified with a URL
   * @param {string} from the path of the file loading this resource,
   *   relative to the WPT folder.
   * @param {string} url the url of the resource being loaded.
   * @returns {Promise<{
   *   ok: string,
   *   arrayBuffer: function(): Buffer,
   *   json: function(): object,
   *   text: function(): string,
   * }>}
   */
  async readAsFetch(from, url) {
    const file = this.toRealFilePath(from, url);
    const data = await fsPromises.readFile(file);
    return {
      ok: true,
      arrayBuffer() { return data.buffer; },
      bytes() { return new Uint8Array(data); },
      json() { return JSON.parse(data.toString()); },
      text() { return data.toString(); },
    };
  }
}

class StatusRule {
  constructor(key, value, pattern) {
    this.key = key;
    this.requires = value.requires || [];
    this.fail = value.fail;
    this.skip = value.skip;
    this.skipTests = value.skipTests;
    if (pattern) {
      this.pattern = this.transformPattern(pattern);
    }
    // TODO(joyeecheung): implement this
    this.scope = value.scope;
    this.comment = value.comment;
  }

  /**
   * Transform a filename pattern into a RegExp
   * @param {string} pattern
   * @returns {RegExp}
   */
  transformPattern(pattern) {
    const result = path.normalize(pattern).replace(/[-/\\^$+?.()|[\]{}]/g, '\\$&');
    return new RegExp(result.replace('*', '.*'));
  }
}

class StatusRuleSet {
  constructor() {
    // We use two sets of rules to speed up matching
    this.exactMatch = {};
    this.patternMatch = [];
  }

  /**
   * @param {object} rules
   */
  addRules(rules) {
    for (const key of Object.keys(rules)) {
      if (key.includes('*')) {
        this.patternMatch.push(new StatusRule(key, rules[key], key));
      } else {
        const normalizedPath = path.normalize(key);
        this.exactMatch[normalizedPath] = new StatusRule(key, rules[key]);
      }
    }
  }

  match(file) {
    const result = [];
    const exact = this.exactMatch[file];
    if (exact) {
      result.push(exact);
    }
    for (const item of this.patternMatch) {
      if (item.pattern.test(file)) {
        result.push(item);
      }
    }
    return result;
  }
}

// A specification of WPT test
class WPTTestSpec {
  #content;

  /**
   * @param {string} mod name of the WPT module, e.g.
   *   'html/webappapis/microtask-queuing'
   * @param {string} filename path of the test, relative to mod, e.g.
   *   'test.any.js'
   * @param {StatusRule[]} rules
   * @param {string} variant test file variant
   * @param {'window'|'dedicatedworker'} [globalScope] generated test global
   */
  constructor(mod, filename, rules, variant = '', globalScope) {
    this.module = mod;
    this.filename = filename;
    this.variant = variant;
    this.globalScope = globalScope;
    this.rules = [...new Set(rules)];

    this.requires = new Set();
    this.failedTests = [];
    this.flakyTests = [];
    this.skipReasons = [];
    this.skippedTests = [];
    for (const item of this.rules) {
      if (item.requires.length) {
        for (const req of item.requires) {
          this.requires.add(req);
        }
      }
      if (Array.isArray(item.fail?.expected)) {
        this.failedTests.push(...item.fail.expected);
      }
      if (Array.isArray(item.fail?.flaky)) {
        this.failedTests.push(...item.fail.flaky);
        this.flakyTests.push(...item.fail.flaky);
      }
      if (item.skip) {
        this.skipReasons.push(item.skip);
      }
      if (Array.isArray(item.skipTests)) {
        this.skippedTests.push(...item.skipTests);
      }
    }

    this.failedTests = [...new Set(this.failedTests)];
    this.flakyTests = [...new Set(this.flakyTests)];
    this.skipReasons = [...new Set(this.skipReasons)];
  }

  /**
   * @param {string} mod
   * @param {string} filename
   * @param {StatusRule[]} rules
   * @param {(spec: WPTTestSpec) => StatusRule[]} [getAdditionalRules]
   * @returns {WPTTestSpec[]}
   */
  static from(mod, filename, rules, getAdditionalRules) {
    const spec = new WPTTestSpec(mod, filename, rules);
    const meta = spec.getMeta();
    const variants = meta.variant || [''];
    const createSpec = (variant, globalScope) => {
      let result = new WPTTestSpec(mod, filename, rules, variant, globalScope);
      const additionalRules = getAdditionalRules?.(result) || [];
      if (additionalRules.length > 0) {
        result = new WPTTestSpec(
          mod,
          filename,
          [...new Set([...rules, ...additionalRules])],
          variant,
          globalScope,
        );
      }
      return result;
    };

    if (!spec.isAnyTest()) {
      return variants.map((variant) => createSpec(variant));
    }

    const requestedGlobals = meta.global ?
      meta.global.split(',').map((item) => item.trim()) :
      ['window', 'dedicatedworker'];
    const supportedGlobals = new Set();
    for (const globalScope of requestedGlobals) {
      if (globalScope === 'window' || globalScope === 'dedicatedworker') {
        supportedGlobals.add(globalScope);
      } else if (globalScope === 'worker') {
        supportedGlobals.add('dedicatedworker');
      }
    }

    return ['window', 'dedicatedworker']
      .filter((globalScope) => supportedGlobals.has(globalScope))
      .flatMap((globalScope) => variants.map(
        (variant) => createSpec(variant, globalScope)));
  }

  /**
   * @returns {boolean}
   */
  isAnyTest() {
    return /\.any\.js$/.test(this.filename);
  }

  /**
   * @returns {boolean}
   */
  isWebWorkerTest() {
    return /\.worker\.js$/.test(this.filename) ||
      this.globalScope === 'dedicatedworker';
  }

  /**
   * Check if a subtest should be skipped by name.
   * @param {string} name
   * @returns {boolean}
   */
  isSkippedTest(name) {
    for (const matcher of this.skippedTests) {
      if (typeof matcher === 'string') {
        if (name === matcher) return true;
      } else if (matcher.test(name)) {
        return true;
      }
    }
    return false;
  }

  getRelativePath() {
    return path.join(this.module, this.filename);
  }

  getStatusKey() {
    if (this.globalScope === 'dedicatedworker') {
      return this.filename.replace(/\.any\.js$/, '.any.worker.html');
    }
    if (this.globalScope === 'window') {
      return this.filename.replace(/\.any\.js$/, '.any.html');
    }
    return this.filename;
  }

  getTestPath() {
    let testPath = path.join(this.module, this.getStatusKey());
    testPath = testPath.replace(/\.js$/, '.html');
    return `${testPath.split(path.sep).join('/')}${this.variant}`;
  }

  /**
   * Whether a command line argument selects this spec. Accepts the source file
   * name, which selects every global and variant generated from it, or a test
   * path as printed alongside the results, which selects only this one.
   * @param {string} arg
   * @returns {boolean}
   */
  isSelectedBy(arg) {
    if (arg === this.getTestPath()) {
      return true;
    }
    const [filename, variant = ''] = arg.split('?');
    // Spec filenames are relative paths, so they use the platform separator,
    // while the argument is written with forward slashes.
    return filename.split(path.sep).join('/') ===
      this.filename.split(path.sep).join('/') &&
      (!variant || this.variant.substring(1) === variant);
  }

  getAbsolutePath() {
    return fixtures.path('wpt', this.getRelativePath());
  }

  /**
   * @returns {string}
   */
  getContent() {
    this.#content ||= fs.readFileSync(this.getAbsolutePath(), 'utf8');
    return this.#content;
  }

  /**
   * @returns {{ script?: string[]; variant?: string[]; [key: string]: string }} parsed META tags of a spec file
   */
  getMeta() {
    // Like upstream, tolerate missing whitespace around "META:".
    // Refs: https://github.com/web-platform-tests/wpt/blob/master/tools/manifest/sourcefile.py
    const matches = this.getContent().match(/\/\/\s*META:\s*.+/g);
    if (!matches) {
      return {};
    }
    const result = {};
    for (const match of matches) {
      const parts = match.match(/\/\/\s*META:\s*([^=]+?)=(.+)/);
      const key = parts[1];
      const value = parts[2];
      if (key === 'script' || key === 'variant') {
        if (result[key]) {
          result[key].push(value);
        } else {
          result[key] = [value];
        }
      } else {
        result[key] = value;
      }
    }
    return result;
  }
}

const kIntlRequirement = {
  none: 0,
  small: 1,
  full: 2,
  // TODO(joyeecheung): we may need to deal with --with-intl=system-icu
};

class BuildRequirement {
  constructor() {
    this.currentIntl = kIntlRequirement.none;
    if (process.config.variables.v8_enable_i18n_support === 0) {
      this.currentIntl = kIntlRequirement.none;
      return;
    }
    // i18n enabled
    if (process.config.variables.icu_small) {
      this.currentIntl = kIntlRequirement.small;
    } else {
      this.currentIntl = kIntlRequirement.full;
    }
    // Not using common.hasCrypto because of the global leak checks
    this.hasCrypto = Boolean(process.versions.openssl) &&
      !process.env.NODE_SKIP_CRYPTO;

    // Not using common.hasInspector because of the global leak checks
    this.hasInspector = Boolean(process.features.inspector);
  }

  /**
   * @param {Set} requires
   * @returns {string|false} The config that the build is lacking, or false
   */
  isLacking(requires) {
    const current = this.currentIntl;
    if (requires.has('full-icu') && current !== kIntlRequirement.full) {
      return 'full-icu';
    }
    if (requires.has('small-icu') && current < kIntlRequirement.small) {
      return 'small-icu';
    }
    if (requires.has('crypto') && !this.hasCrypto) {
      return 'crypto';
    }
    if (requires.has('inspector') && !this.hasInspector) {
      return 'inspector';
    }
    return false;
  }
}

const buildRequirements = new BuildRequirement();

class StatusLoader {
  /**
   * @param {string} path relative path of the WPT subset
   */
  constructor(path) {
    this.path = path;
    this.rules = new StatusRuleSet();
    /** @type {WPTTestSpec[]} */
    this.specs = [];
  }

  /**
   * Grep for all .*.js file recursively in a directory.
   * @param {string} dir
   * @returns {any[]}
   */
  grep(dir) {
    let result = [];
    const list = fs.readdirSync(dir);
    for (const file of list) {
      const filepath = path.join(dir, file);
      const stat = fs.statSync(filepath);
      if (stat.isDirectory()) {
        if (wptNonTestDirs.has(file)) {
          continue;
        }
        const list = this.grep(filepath);
        result = result.concat(list);
      } else {
        if (!(/\.\w+\.js$/.test(filepath))) {
          continue;
        }
        result.push(filepath);
      }
    }
    return result;
  }

  load() {
    const dir = path.join(__dirname, '..', 'wpt');
    let result;

    try {
      this.statusFile = `${this.path}.json`;
      const jsonFile = path.join(dir, 'status', this.statusFile);
      result = JSON.parse(fs.readFileSync(jsonFile, 'utf8'));
    } catch (err) {
      if (err?.code !== 'ENOENT') throw err;
      this.statusFile = `${this.path}.cjs`;
      result = require(path.join(dir, 'status', this.statusFile));
    }

    this.rules.addRules(result);

    const subDir = fixtures.path('wpt', this.path);
    const list = this.grep(subDir);
    for (const file of list) {
      const relativePath = path.relative(subDir, file);
      const match = this.rules.match(relativePath);
      this.specs.push(...WPTTestSpec.from(
        this.path,
        relativePath,
        match,
        (spec) => [
          ...this.rules.match(spec.getStatusKey()),
          ...this.rules.match(`${spec.getStatusKey()}${spec.variant}`),
        ],
      ));
    }
  }
}

const kPass = 'pass';
const kFail = 'fail';
const kSkip = 'skip';
const kTimeout = 'timeout';
const kIncomplete = 'incomplete';
const kUncaught = 'uncaught';
const NODE_UNCAUGHT = 100;

const limit = (concurrency) => {
  let running = 0;
  const queue = [];

  const execute = async ({ fn, resolve, reject }) => {
    running++;
    try {
      resolve(await fn());
    } catch (err) {
      reject(err);
    } finally {
      running--;
      if (queue.length > 0) {
        execute(queue.shift());
      }
    }
  };

  return (fn) => new Promise((resolve, reject) => {
    const task = { fn, resolve, reject };
    if (running < concurrency) {
      execute(task);
    } else {
      queue.push(task);
    }
  });
};

function isUnexpectedPass(spec, name) {
  return spec.failedTests.includes(name) && !spec.flakyTests.includes(name);
}

function getUnexpectedPasses(queue, results) {
  const unexpectedPasses = [];
  for (const spec of queue) {
    const key = spec.getStatusKey();
    if (results[key]?.skip) {
      continue;
    }

    for (const expectedToFail of spec.failedTests) {
      if (isUnexpectedPass(spec, expectedToFail) &&
          !results[key]?.fail?.expected?.includes(expectedToFail)) {
        unexpectedPasses.push(`${key}:${expectedToFail}`);
      }
    }
  }
  return unexpectedPasses;
}

function getHarnessErrorName(harnessStatus) {
  if (typeof harnessStatus.stack === 'string') {
    const name = harnessStatus.stack.split('\n', 1)[0];
    if (name) {
      return name;
    }
  }
  return harnessStatus.message || 'WPT test harness error';
}

/**
 * @typedef {object} SpecHandlers
 * @property {(message: object) => void} message Handles a message from the spec.
 * @property {(failure: { name: string, message: string, stack: string })
 *   => boolean} failure Reports a spec that died without completing. Returns
 *   false when the spec had already finished and the failure was ignored.
 */

/**
 * @typedef {object} SpecHandle
 * @property {() => void} kill Forces the spec to stop running.
 * @property {Promise<unknown>} finished Settles once the spec has stopped.
 */

/**
 * Run a spec on a worker thread.
 * @param {string[]} execArgv
 * @param {object} workerData
 * @param {SpecHandlers} handlers
 * @returns {SpecHandle}
 */
function runSpecOnThread(execArgv, workerData, handlers) {
  const worker = new Worker(workerPath, { execArgv, workerData });
  worker.on('message', handlers.message);
  worker.on('error', (err) => handlers.failure({
    name: `${err}`,
    message: err.message,
    stack: inspect(err),
  }));
  return {
    kill: () => worker.terminate(),
    finished: events.once(worker, 'exit').catch(() => {}),
  };
}

/**
 * Run a spec in a child process, so that a spec crashing the process only
 * takes down its own run and the runner can attribute the crash to it.
 * @param {string[]} execArgv
 * @param {object} workerData
 * @param {SpecHandlers} handlers
 * @returns {SpecHandle}
 */
function runSpecInProcess(execArgv, workerData, handlers) {
  const forwardStderr = execArgv.some(
    (flag) => flag === '--inspect-brk' || flag.startsWith('--inspect-brk='));
  const child = fork(workerPath, {
    execArgv,
    // Status files may skip subtests by regular expression, which JSON
    // serialization would not preserve.
    serialization: 'advanced',
    stdio: ['ignore', 'inherit', 'pipe', 'ipc'],
  });
  child.send(workerData);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (chunk) => {
    stderr += chunk;
    if (forwardStderr) {
      process.stderr.write(chunk);
    }
  });

  child.on('message', (message) => {
    // The spec reports uncaught errors itself so that they are named the same
    // way as they would be on the worker thread backend.
    if (message.type === 'uncaught') {
      handlers.failure(message.error);
      return;
    }
    handlers.message(message);
  });
  child.on('error', (err) => handlers.failure({
    name: `${err}`,
    message: err.message,
    stack: inspect(err),
  }));
  // `close` rather than `exit` so that everything the process wrote to stderr
  // on its way out is part of the reported failure.
  child.on('close', (code, signal) => {
    const name = signal ?
      `Test process was killed by signal ${signal}` :
      `Test process exited with code ${code}`;
    if (!handlers.failure({ name, message: name, stack: stderr }) &&
        stderr && !forwardStderr) {
      process.stderr.write(stderr);
    }
  });

  return {
    kill: () => child.kill('SIGKILL'),
    finished: events.once(child, 'close').catch(() => {}),
  };
}

const backends = {
  __proto__: null,
  thread: runSpecOnThread,
  process: runSpecInProcess,
};

class WPTRunner {
  constructor(path, {
    concurrency = os.availableParallelism() - 1 || 1,
    backend = 'thread',
  } = {}) {
    if (!Number.isInteger(concurrency) || concurrency < 1) {
      throw new TypeError('WPT concurrency must be a positive integer');
    }

    // RISC-V has very limited virtual address space in the currently common
    // sv39 mode, in which we can only create a very limited number of wasm
    // memories(27 from a fresh node repl). Limit the concurrency to avoid
    // creating too many wasm memories that would fail.
    if (process.arch === 'riscv64' || process.arch === 'riscv32') {
      concurrency = Math.min(10, concurrency);
    }

    this.inspectBrk = process.env.WPT_INSPECT !== undefined;

    // The override exists so that every suite can be run either way without
    // editing the drivers, which is how the two backends are kept compatible.
    if (this.inspectBrk) {
      backend = 'process';
    } else {
      backend = process.env.WPT_BACKEND || backend;
    }
    this.runSpec = backends[backend];
    if (this.runSpec === undefined) {
      throw new Error(`Invalid WPT backend ${backend}, expected one of ` +
                      `${Object.keys(backends).join(', ')}`);
    }

    this.path = path;
    this.resource = new ResourceLoader(path);
    this.concurrency = concurrency;

    // Since we need to prepare the Web Worker APIs
    // in the harness that runs on all WPT workers,
    // we enable the API globally. This has no practical
    // effect on the non-web-worker tests, however.
    this.flags = ['--experimental-web-worker'];
    if (this.inspectBrk) {
      this.flags.push('--inspect-brk=0');
    }
    this.globalThisInitScripts = [];
    this.initScript = null;

    this.status = new StatusLoader(path);
    this.status.load();
    this.statusFile = this.status.statusFile;
    this.specs = new Set(this.status.specs);

    this.results = {};
    this.inProgress = new Set();
    this.handles = new Map();
    this.unexpectedFailures = [];
    this.skippedSpecCount = 0;

    this.subtestCounts = { passed: 0, failed: 0, expectedFailures: 0, skipped: 0, unexpectedPasses: 0 };

    if (process.env.WPT_REPORT != null) {
      this.report = new WPTReport(path);
    }
  }

  /**
   * Sets the Node.js flags passed to the worker.
   * @param {string[]} flags
   */
  setFlags(flags) {
    this.flags = this.flags.concat(flags);
  }

  /**
   * Sets a script to be run in the worker before executing the tests.
   * @param {string} script
   */
  setInitScript(script) {
    this.initScript = script;
  }

  /**
   * Set the scripts modifier for each script.
   * @param {(meta: { code: string, filename: string }) => void} modifier
   */
  setScriptModifier(modifier) {
    this.scriptsModifier = modifier;
  }

  /**
   * @param {WPTTestSpec} spec
   * @returns {string}
   */
  fullInitScript(spec) {
    const url = new URL(`/${spec.getTestPath()}`, 'http://wpt');
    const title = spec.getMeta().title;
    let { initScript } = this;

    initScript = `${initScript}\n\n//===\nglobalThis.location = new URL("${url.href}");`;

    if (title) {
      initScript = `${initScript}\n\n//===\nglobalThis.META_TITLE = "${title}";`;
    }

    if (this.globalThisInitScripts.length === null) {
      return initScript;
    }

    if (spec.isWebWorkerTest()) {
      return initScript;
    }

    const globalThisInitScript = this.globalThisInitScripts.join('\n\n//===\n');

    if (initScript === null) {
      return globalThisInitScript;
    }

    return `${globalThisInitScript}\n\n//===\n${initScript}`;
  }

  /**
   * Pretend the runner is run in `name`'s environment (globalThis).
   * @param {'Window'} name
   * @see {@link https://github.com/nodejs/node/blob/24673ace8ae196bd1c6d4676507d6e8c94cf0b90/test/fixtures/wpt/resources/idlharness.js#L654-L671}
   */
  pretendGlobalThisAs(name) {
    switch (name) {
      case 'Window': {
        this.globalThisInitScripts.push('globalThis.Window = Object.getPrototypeOf(globalThis).constructor;');
        break;
      }

      // TODO(XadillaX): implement `ServiceWorkerGlobalScope`,
      // `DedicateWorkerGlobalScope`, etc.
      //
      // e.g. `ServiceWorkerGlobalScope` should implement dummy
      // `addEventListener` and so on.

      default: throw new Error(`Invalid globalThis type ${name}.`);
    }
  }

  // TODO(joyeecheung): work with the upstream to port more tests in .html
  // to .js.
  async runJsTests() {
    const queue = this.buildQueue();

    const run = limit(this.concurrency);
    const jobs = [];

    for (const spec of queue) {
      const content = spec.getContent();
      const meta = spec.getMeta(content);

      const absolutePath = spec.getAbsolutePath();
      const relativePath = spec.getRelativePath();
      const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
      // *.worker.js tests are dedicated worker tests by definition. Each
      // dedicated worker variant generated from a multi-global (*.any.js)
      // test also runs inside an actual Web Worker. Refs:
      // https://web-platform-tests.org/writing-tests/testharness.html#multi-global-tests
      const isAnyTest = spec.isAnyTest();
      const isWebWorkerTest = spec.isWebWorkerTest();

      // Scripts specified with the `// META: script=` header. For tests
      // that run inside a Web Worker they are imported by the worker
      // instead.
      const scriptsToRun = isWebWorkerTest ? [] : meta.script?.map((script) => {
        const obj = {
          filename: this.resource.toRealFilePath(relativePath, script),
          code: this.resource.read(relativePath, script),
        };
        this.scriptsModifier?.(obj);
        return obj;
      }) ?? [];
      if (!isWebWorkerTest) {
        // The actual test
        const obj = {
          code: content,
          filename: absolutePath,
        };
        this.scriptsModifier?.(obj);
        scriptsToRun.push(obj);
      }

      jobs.push(run(async () => {
        this.inProgress.add(spec);
        const reportResult = this.report?.getResult(spec);

        const handle = this.runSpec(this.flags, {
          testRelativePath: relativePath,
          wptRunner: __filename,
          wptPath: this.path,
          initScript: this.fullInitScript(spec),
          harness: {
            code: fs.readFileSync(harnessPath, 'utf8'),
            filename: harnessPath,
          },
          scriptsToRun,
          // Set when the test runs inside an actual Web Worker.
          webWorker: isWebWorkerTest ? {
            path: absolutePath,
            isAnyTest,
            initScript: this.initScript,
            title: meta.title,
            variant: spec.variant,
            scripts: meta.script?.map(
              (script) => this.resource.toRealFilePath(relativePath, script),
            ) ?? [],
            skippedTests: spec.skippedTests,
          } : undefined,
          needsGc: !!meta.script?.find((script) => script === '/common/gc.js'),
          skippedTests: spec.skippedTests,
        }, {
          message: (message) => {
            switch (message.type) {
              case 'result':
                return this.resultCallback(spec, message.result, reportResult);
              case 'skip':
                return this.skipTest(spec, { name: message.name }, reportResult);
              case 'completion':
                return this.completionCallback(spec, message.status, reportResult);
              default:
                throw new Error(`Unexpected message from spec runner: ${message.type}`);
            }
          },
          failure: (failure) => {
            if (!this.inProgress.has(spec)) {
              // The test is already finished. Ignore anything that happens
              // after it, including the runner terminating it itself.
              return false;
            }
            // Generate a subtest failure for visibility.
            // No need to record this synthetic failure with wpt.fyi.
            this.fail(spec, { status: NODE_UNCAUGHT, ...failure }, kUncaught);
            // Mark the whole test as failed in wpt.fyi report.
            reportResult?.finish('ERROR');
            this.inProgress.delete(spec);
            this.report?.write();
            return true;
          },
        });
        this.handles.set(spec, handle);

        await handle.finished;
      }));
    }

    process.on('exit', () => {
      for (const spec of this.inProgress) {
        // No need to record this synthetic failure with wpt.fyi.
        this.fail(spec, { name: 'Incomplete' }, kIncomplete);
        // Mark the whole test as failed in wpt.fyi report.
        const reportResult = this.report?.getResult(spec);
        reportResult?.finish('ERROR');
      }
      inspect.defaultOptions.depth = Infinity;
      // Sorts the rules to have consistent output
      console.log('');
      console.log(JSON.stringify(Object.keys(this.results).sort().reduce(
        (obj, key) => {
          obj[key] = this.results[key];
          return obj;
        },
        {},
      ), null, 2));

      const failures = [];
      let expectedFailures = 0;
      for (const [key, item] of Object.entries(this.results)) {
        if (item.fail?.unexpected) {
          failures.push(key);
        }
        if (item.fail?.expected) {
          expectedFailures++;
        }
      }

      const unexpectedPasses = getUnexpectedPasses(queue, this.results);

      // Write the report on clean exit. The report is also written
      // incrementally after each spec completes (see completionCallback)
      // so that results survive if the process is killed.
      this.report?.write();

      const p = (n, word, suffix = 's') => `${n} ${word}${n === 1 ? '' : suffix}`;
      const ran = queue.length;
      const skipped = this.skippedSpecCount;
      const total = ran + skipped;
      const passed = ran - expectedFailures - failures.length;
      const { subtestCounts } = this;
      console.log('');
      console.log(`Files: ${ran}/${total} ran, ${passed} passed,`,
                  `${skipped} skipped, ${p(expectedFailures, 'expected failure')},`,
                  `${p(failures.length, 'unexpected failure')},`,
                  `${p(unexpectedPasses.length, 'unexpected pass', 'es')}`);
      console.log(`Subtests: ${subtestCounts.passed} passed,`,
                  `${subtestCounts.skipped} skipped, ${p(subtestCounts.expectedFailures, 'expected failure')},`,
                  `${p(subtestCounts.failed, 'unexpected failure')},`,
                  `${p(subtestCounts.unexpectedPasses, 'unexpected pass', 'es')}`);
      if (failures.length > 0) {
        const file = path.join('test', 'wpt', 'status', this.statusFile);
        throw new Error(
          `Found ${failures.length} unexpected failures. ` +
          `Consider updating ${file} for these files:\n${failures.join('\n')}`);
      }
      if (unexpectedPasses.length > 0) {
        const file = path.join('test', 'wpt', 'status', this.statusFile);
        throw new Error(
          `Found ${unexpectedPasses.length} unexpected passes. ` +
          `Consider updating ${file} for these files:\n${unexpectedPasses.join('\n')}`);
      }
    });

    // Promises do not keep the event loop alive. Keep a referenced handle
    // until every queued spec has run, including the gap between terminating
    // one spec runner and receiving its exit event.
    const keepAlive = setInterval(() => {}, 2 ** 31 - 1);
    try {
      const outcomes = await Promise.allSettled(jobs);
      const errors = outcomes
        .filter((outcome) => outcome.status === 'rejected')
        .map((outcome) => outcome.reason);
      if (errors.length === 1) {
        throw errors[0];
      }
      if (errors.length > 1) {
        throw new AggregateError(errors, 'Multiple WPT spec runners failed');
      }
    } finally {
      clearInterval(keepAlive);
    }
  }

  // Map WPT test status to strings
  getTestStatus(status) {
    switch (status) {
      case 1:
        return kFail;
      case 2:
        return kTimeout;
      case 3:
        return kIncomplete;
      case NODE_UNCAUGHT:
        return kUncaught;
      default:
        return kPass;
    }
  }

  /**
   * Report the status of each specific test case (there could be multiple
   * in one test file).
   * @param {WPTTestSpec} spec
   * @param {Test} test The Test object returned by WPT harness
   * @param {ReportResult} reportResult The report result object
   */
  resultCallback(spec, test, reportResult) {
    const status = this.getTestStatus(test.status);
    if (status !== kPass) {
      this.fail(spec, test, status, reportResult);
    } else {
      this.succeed(spec, test, status, reportResult);
    }
  }

  /**
   * Report the status of each WPT test (one per file)
   * @param {WPTTestSpec} spec
   * @param {object} harnessStatus - The status object returned by WPT harness.
   * @param {ReportResult} reportResult The report result object
   */
  completionCallback(spec, harnessStatus, reportResult) {
    const status = this.getTestStatus(harnessStatus.status);

    // Treat it like a test case failure
    if (status === kTimeout) {
      // No need to record this synthetic failure with wpt.fyi.
      this.fail(spec, { name: 'WPT testharness timeout' }, kTimeout);
      // Mark the whole test as TIMEOUT in wpt.fyi report.
      reportResult?.finish('TIMEOUT');
    } else if (status !== kPass) {
      // No need to record this synthetic failure with wpt.fyi.
      this.fail(spec, {
        status: status,
        name: getHarnessErrorName(harnessStatus),
        message: harnessStatus.message,
        stack: harnessStatus.stack,
      }, status);
      // Mark the whole test as ERROR in wpt.fyi report.
      reportResult?.finish('ERROR');
    } else {
      reportResult?.finish();
    }
    this.inProgress.delete(spec);
    // Write report incrementally so results survive even if the process
    // is killed before the exit handler runs.
    this.report?.write();
    // Always force termination of the spec runner. Some tests allocate
    // resources that would otherwise keep it alive.
    this.handles.get(spec).kill();
  }

  addTestResult(spec, item) {
    const key = spec.getStatusKey();
    let result = this.results[key];
    result ||= this.results[key] = {};
    if (item.status === kSkip) {
      if (item.name) {
        // Subtest-level skip: { filename: { skipTests: [ ... ] } }
        result.skipTests ||= [];
        if (!result.skipTests.includes(item.name)) {
          result.skipTests.push(item.name);
        }
      } else {
        // File-level skip: { filename: { skip: 'reason' } }
        result[kSkip] = item.reason;
      }
    } else {
      // { filename: { fail: { expected: [ ... ],
      //                      unexpected: [ ... ] } }}
      result[item.status] ||= {};
      const key = item.expected ? 'expected' : 'unexpected';
      result[item.status][key] ||= [];
      const hasName = result[item.status][key].includes(item.name);
      if (!hasName) {
        result[item.status][key].push(item.name);
      }
    }
  }

  succeed(spec, test, status, reportResult) {
    const unexpectedPass = isUnexpectedPass(spec, test.name);
    if (unexpectedPass) {
      console.log(`[UNEXPECTED_PASS][${status.toUpperCase()}] ${spec.getTestPath()}: ${test.name}`);
      this.subtestCounts.unexpectedPasses++;
    } else {
      console.log(`[${status.toUpperCase()}] ${spec.getTestPath()}: ${test.name}`);
      this.subtestCounts.passed++;
    }
    reportResult?.addSubtest(test.name, 'PASS');
  }

  skipTest(spec, test, reportResult) {
    console.log(`[SKIP] ${spec.getTestPath()}: ${test.name}`);
    reportResult?.addSubtest(test.name, 'NOTRUN');
    this.subtestCounts.skipped++;
    this.addTestResult(spec, {
      name: test.name,
      status: kSkip,
    });
  }

  fail(spec, test, status, reportResult) {
    const expected = spec.failedTests.includes(test.name);
    if (expected) {
      console.log(`[EXPECTED_FAILURE][${status.toUpperCase()}] ${spec.getTestPath()}: ${test.name}`);
    } else {
      console.log(`[UNEXPECTED_FAILURE][${status.toUpperCase()}] ${spec.getTestPath()}: ${test.name}`);
    }
    if (status === kFail || status === kUncaught) {
      console.log(test.message);
      console.log(test.stack);
    }
    const command = [
      process.execPath,
      ...process.execArgv,
      require.main?.filename,
      `'${spec.getTestPath()}'`,
    ].join(' ');
    console.log(`Command: ${command}\n`);

    reportResult?.addSubtest(test.name, 'FAIL', test.message);
    if (expected) {
      this.subtestCounts.expectedFailures++;
    } else {
      this.subtestCounts.failed++;
    }

    this.addTestResult(spec, {
      name: test.name,
      expected,
      status: kFail,
      reason: test.message || status,
    });
  }

  skip(spec, reasons) {
    const joinedReasons = reasons.join('; ');
    console.log(`[SKIPPED] ${spec.getTestPath()}: ${joinedReasons}`);
    this.skippedSpecCount++;
    this.addTestResult(spec, {
      status: kSkip,
      reason: joinedReasons,
    });
  }

  buildQueue() {
    const queue = [];
    this.skippedSpecCount = 0;
    const arg = process.argv[2];
    if (this.inspectBrk && !arg) {
      throw new Error('WPT_INSPECT requires a WPT test path');
    }
    for (const spec of this.specs) {
      if (arg) {
        if (spec.isSelectedBy(arg)) {
          queue.push(spec);
        }
        continue;
      }

      if (spec.skipReasons.length > 0) {
        this.skip(spec, spec.skipReasons);
        continue;
      }

      const lackingSupport = buildRequirements.isLacking(spec.requires);
      if (lackingSupport) {
        this.skip(spec, [ `requires ${lackingSupport}` ]);
        continue;
      }

      queue.push(spec);
    }

    // If the tests are run as `node test/wpt/test-something.js subset.any.js`,
    // only `subset.any.js` (all variants and globals) will be run by the runner.
    // If the tests are run as `node test/wpt/test-something.js 'subset.any.js?1-10'`,
    // only the `?1-10` variant of `subset.any.js` will be run by the runner.
    // A test path as printed with the results, e.g.
    // `'dir/subset.any.worker.html?1-10'`, runs exactly that one.
    if (arg && queue.length === 0) {
      throw new Error(`${arg} not found!`);
    }
    if (this.inspectBrk && queue.length !== 1) {
      const matches = queue.map((spec) => spec.getTestPath()).join('\n');
      throw new Error(
        `WPT_INSPECT requires exactly one generated WPT test path; ` +
        `${arg} matched ${queue.length}:\n${matches}`,
      );
    }
    if (this.inspectBrk && queue[0].isWebWorkerTest()) {
      throw new Error(
        `WPT_INSPECT does not support worker tests: ${queue[0].getTestPath()}`,
      );
    }

    return queue;
  }
}

module.exports = {
  backends,
  getHarnessErrorName,
  getUnexpectedPasses,
  harness: harnessMock,
  isUnexpectedPass,
  ResourceLoader,
  WPTTestSpec,
  WPTRunner,
};
