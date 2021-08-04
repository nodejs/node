'use strict';

const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const { inspect } = require('util');
const { Worker } = require('worker_threads');

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
  }
};

class ResourceLoader {
  constructor(path) {
    this.path = path;
  }

  toRealFilePath(from, url) {
    // We need to patch this to load the WebIDL parser
    url = url.replace(
      '/resources/WebIDLParser.js',
      '/resources/webidl2/lib/webidl2.js'
    );
    const base = path.dirname(from);
    return url.startsWith('/') ?
      fixtures.path('wpt', url) :
      fixtures.path('wpt', base, url);
  }

  /**
   * Load a resource in test/fixtures/wpt specified with a URL
   * @param {string} from the path of the file loading this resource,
   *                      relative to thw WPT folder.
   * @param {string} url the url of the resource being loaded.
   * @param {boolean} asPromise if true, return the resource in a
   *                            pseudo-Response object.
   */
  read(from, url, asFetch = true) {
    const file = this.toRealFilePath(from, url);
    if (asFetch) {
      return fsPromises.readFile(file)
        .then((data) => {
          return {
            ok: true,
            json() { return JSON.parse(data.toString()); },
            text() { return data.toString(); }
          };
        });
    }
    return fs.readFileSync(file, 'utf8');
  }
}

class StatusRule {
  constructor(key, value, pattern = undefined) {
    this.key = key;
    this.requires = value.requires || [];
    this.fail = value.fail;
    this.skip = value.skip;
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
  /**
   * @param {string} mod name of the WPT module, e.g.
   *                     'html/webappapis/microtask-queuing'
   * @param {string} filename path of the test, relative to mod, e.g.
   *                          'test.any.js'
   * @param {StatusRule[]} rules
   */
  constructor(mod, filename, rules) {
    this.module = mod;
    this.filename = filename;

    this.requires = new Set();
    this.failReasons = [];
    this.skipReasons = [];
    for (const item of rules) {
      if (item.requires.length) {
        for (const req of item.requires) {
          this.requires.add(req);
        }
      }
      if (item.fail) {
        this.failReasons.push(item.fail);
      }
      if (item.skip) {
        this.skipReasons.push(item.skip);
      }
    }
  }

  getRelativePath() {
    return path.join(this.module, this.filename);
  }

  getAbsolutePath() {
    return fixtures.path('wpt', this.getRelativePath());
  }

  getContent() {
    return fs.readFileSync(this.getAbsolutePath(), 'utf8');
  }
}

const kIntlRequirement = {
  none: 0,
  small: 1,
  full: 2,
  // TODO(joyeecheung): we may need to deal with --with-intl=system-icu
};

class IntlRequirement {
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
    return false;
  }
}

const intlRequirements = new IntlRequirement();

class StatusLoader {
  /**
   * @param {string} path relative path of the WPT subset
   */
  constructor(path) {
    this.path = path;
    this.loaded = false;
    this.rules = new StatusRuleSet();
    /** @type {WPTTestSpec[]} */
    this.specs = [];
  }

  /**
   * Grep for all .*.js file recursively in a directory.
   * @param {string} dir
   */
  grep(dir) {
    let result = [];
    const list = fs.readdirSync(dir);
    for (const file of list) {
      const filepath = path.join(dir, file);
      const stat = fs.statSync(filepath);
      if (stat.isDirectory()) {
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
    const statusFile = path.join(dir, 'status', `${this.path}.json`);
    const result = JSON.parse(fs.readFileSync(statusFile, 'utf8'));
    this.rules.addRules(result);

    const subDir = fixtures.path('wpt', this.path);
    const list = this.grep(subDir);
    for (const file of list) {
      const relativePath = path.relative(subDir, file);
      const match = this.rules.match(relativePath);
      this.specs.push(new WPTTestSpec(this.path, relativePath, match));
    }
    this.loaded = true;
  }
}

const kPass = 'pass';
const kFail = 'fail';
const kSkip = 'skip';
const kTimeout = 'timeout';
const kIncomplete = 'incomplete';
const kUncaught = 'uncaught';
const NODE_UNCAUGHT = 100;

class WPTRunner {
  constructor(path) {
    this.path = path;
    this.resource = new ResourceLoader(path);

    this.flags = [];
    this.initScript = null;

    this.status = new StatusLoader(path);
    this.status.load();
    this.specMap = new Map(
      this.status.specs.map((item) => [item.filename, item])
    );

    this.results = {};
    this.inProgress = new Set();
    this.workers = new Map();
    this.unexpectedFailures = [];
  }

  /**
   * Sets the Node.js flags passed to the worker.
   * @param {Array<string>} flags
   */
  setFlags(flags) {
    this.flags = flags;
  }

  /**
   * Sets a script to be run in the worker before executing the tests.
   * @param {string} script
   */
  setInitScript(script) {
    this.initScript = script;
  }

  // TODO(joyeecheung): work with the upstream to port more tests in .html
  // to .js.
  runJsTests() {
    let queue = [];

    // If the tests are run as `node test/wpt/test-something.js subset.any.js`,
    // only `subset.any.js` will be run by the runner.
    if (process.argv[2]) {
      const filename = process.argv[2];
      if (!this.specMap.has(filename)) {
        throw new Error(`${filename} not found!`);
      }
      queue.push(this.specMap.get(filename));
    } else {
      queue = this.buildQueue();
    }

    this.inProgress = new Set(queue.map((spec) => spec.filename));

    for (const spec of queue) {
      const testFileName = spec.filename;
      const content = spec.getContent();
      const meta = spec.title = this.getMeta(content);

      const absolutePath = spec.getAbsolutePath();
      const relativePath = spec.getRelativePath();
      const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
      const scriptsToRun = [];
      // Scripts specified with the `// META: script=` header
      if (meta.script) {
        for (const script of meta.script) {
          scriptsToRun.push({
            filename: this.resource.toRealFilePath(relativePath, script),
            code: this.resource.read(relativePath, script, false)
          });
        }
      }
      // The actual test
      scriptsToRun.push({
        code: content,
        filename: absolutePath
      });

      const workerPath = path.join(__dirname, 'wpt/worker.js');
      const worker = new Worker(workerPath, {
        execArgv: this.flags,
        workerData: {
          testRelativePath: relativePath,
          wptRunner: __filename,
          wptPath: this.path,
          initScript: this.initScript,
          harness: {
            code: fs.readFileSync(harnessPath, 'utf8'),
            filename: harnessPath,
          },
          scriptsToRun,
        },
      });
      this.workers.set(testFileName, worker);

      worker.on('message', (message) => {
        switch (message.type) {
          case 'result':
            return this.resultCallback(testFileName, message.result);
          case 'completion':
            return this.completionCallback(testFileName, message.status);
          default:
            throw new Error(`Unexpected message from worker: ${message.type}`);
        }
      });

      worker.on('error', (err) => {
        if (!this.inProgress.has(testFileName)) {
          // The test is already finished. Ignore errors that occur after it.
          // This can happen normally, for example in timers tests.
          return;
        }
        this.fail(
          testFileName,
          {
            status: NODE_UNCAUGHT,
            name: 'evaluation in WPTRunner.runJsTests()',
            message: err.message,
            stack: inspect(err)
          },
          kUncaught
        );
        this.inProgress.delete(testFileName);
      });
    }

    process.on('exit', () => {
      const total = this.specMap.size;
      if (this.inProgress.size > 0) {
        for (const filename of this.inProgress) {
          this.fail(filename, { name: 'Unknown' }, kIncomplete);
        }
      }
      inspect.defaultOptions.depth = Infinity;
      console.log(this.results);

      const failures = [];
      let expectedFailures = 0;
      let skipped = 0;
      for (const key of Object.keys(this.results)) {
        const item = this.results[key];
        if (item.fail && item.fail.unexpected) {
          failures.push(key);
        }
        if (item.fail && item.fail.expected) {
          expectedFailures++;
        }
        if (item.skip) {
          skipped++;
        }
      }
      const ran = total - skipped;
      const passed = ran - expectedFailures - failures.length;
      console.log(`Ran ${ran}/${total} tests, ${skipped} skipped,`,
                  `${passed} passed, ${expectedFailures} expected failures,`,
                  `${failures.length} unexpected failures`);
      if (failures.length > 0) {
        const file = path.join('test', 'wpt', 'status', `${this.path}.json`);
        throw new Error(
          `Found ${failures.length} unexpected failures. ` +
          `Consider updating ${file} for these files:\n${failures.join('\n')}`);
      }
    });
  }

  getTestTitle(filename) {
    const spec = this.specMap.get(filename);
    const title = spec.meta && spec.meta.title;
    return title ? `${filename} : ${title}` : filename;
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
   *
   * @param {string} filename
   * @param {Test} test  The Test object returned by WPT harness
   */
  resultCallback(filename, test) {
    const status = this.getTestStatus(test.status);
    const title = this.getTestTitle(filename);
    console.log(`---- ${title} ----`);
    if (status !== kPass) {
      this.fail(filename, test, status);
    } else {
      this.succeed(filename, test, status);
    }
  }

  /**
   * Report the status of each WPT test (one per file)
   *
   * @param {string} filename
   * @param {object} harnessStatus - The status object returned by WPT harness.
   */
  completionCallback(filename, harnessStatus) {
    // Treat it like a test case failure
    if (harnessStatus.status === 2) {
      const title = this.getTestTitle(filename);
      console.log(`---- ${title} ----`);
      this.resultCallback(filename, { status: 2, name: 'Unknown' });
    }
    this.inProgress.delete(filename);
    // Always force termination of the worker. Some tests allocate resources
    // that would otherwise keep it alive.
    this.workers.get(filename).terminate();
  }

  addTestResult(filename, item) {
    let result = this.results[filename];
    if (!result) {
      result = this.results[filename] = {};
    }
    if (item.status === kSkip) {
      // { filename: { skip: 'reason' } }
      result[kSkip] = item.reason;
    } else {
      // { filename: { fail: { expected: [ ... ],
      //                      unexpected: [ ... ] } }}
      if (!result[item.status]) {
        result[item.status] = {};
      }
      const key = item.expected ? 'expected' : 'unexpected';
      if (!result[item.status][key]) {
        result[item.status][key] = [];
      }
      if (result[item.status][key].indexOf(item.reason) === -1) {
        result[item.status][key].push(item.reason);
      }
    }
  }

  succeed(filename, test, status) {
    console.log(`[${status.toUpperCase()}] ${test.name}`);
  }

  fail(filename, test, status) {
    const spec = this.specMap.get(filename);
    const expected = !!(spec.failReasons.length);
    if (expected) {
      console.log(`[EXPECTED_FAILURE][${status.toUpperCase()}] ${test.name}`);
      console.log(spec.failReasons.join('; '));
    } else {
      console.log(`[UNEXPECTED_FAILURE][${status.toUpperCase()}] ${test.name}`);
    }
    if (status === kFail || status === kUncaught) {
      console.log(test.message);
      console.log(test.stack);
    }
    const command = `${process.execPath} ${process.execArgv}` +
                    ` ${require.main.filename} ${filename}`;
    console.log(`Command: ${command}\n`);
    this.addTestResult(filename, {
      expected,
      status: kFail,
      reason: test.message || status
    });
  }

  skip(filename, reasons) {
    const title = this.getTestTitle(filename);
    console.log(`---- ${title} ----`);
    const joinedReasons = reasons.join('; ');
    console.log(`[SKIPPED] ${joinedReasons}`);
    this.addTestResult(filename, {
      status: kSkip,
      reason: joinedReasons
    });
  }

  getMeta(code) {
    const matches = code.match(/\/\/ META: .+/g);
    if (!matches) {
      return {};
    }
    const result = {};
    for (const match of matches) {
      const parts = match.match(/\/\/ META: ([^=]+?)=(.+)/);
      const key = parts[1];
      const value = parts[2];
      if (key === 'script') {
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

  buildQueue() {
    const queue = [];
    for (const spec of this.specMap.values()) {
      const filename = spec.filename;
      if (spec.skipReasons.length > 0) {
        this.skip(filename, spec.skipReasons);
        continue;
      }

      const lackingIntl = intlRequirements.isLacking(spec.requires);
      if (lackingIntl) {
        this.skip(filename, [ `requires ${lackingIntl}` ]);
        continue;
      }

      queue.push(spec);
    }
    return queue;
  }
}

module.exports = {
  harness: harnessMock,
  ResourceLoader,
  WPTRunner
};
