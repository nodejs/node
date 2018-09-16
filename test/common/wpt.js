/* eslint-disable node-core/required-modules */
'use strict';

const assert = require('assert');
const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const vm = require('vm');

// https://github.com/w3c/testharness.js/blob/master/testharness.js
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

  fetch(url, asPromise = true) {
    // We need to patch this to load the WebIDL parser
    url = url.replace(
      '/resources/WebIDLParser.js',
      '/resources/webidl2/lib/webidl2.js'
    );
    const file = url.startsWith('/') ?
      fixtures.path('wpt', url) :
      fixtures.path('wpt', this.path, url);
    if (asPromise) {
      return fsPromises.readFile(file)
        .then((data) => {
          return {
            ok: true,
            json() { return JSON.parse(data.toString()); },
            text() { return data.toString(); }
          };
        });
    } else {
      return fs.readFileSync(file, 'utf8');
    }
  }
}

class WPTTest {
  /**
   * @param {string} mod
   * @param {string} filename
   * @param {string[]} requires
   * @param {string | undefined} failReason
   * @param {string | undefined} skipReason
   */
  constructor(mod, filename, requires, failReason, skipReason) {
    this.module = mod; // name of the WPT module, e.g. 'url'
    this.filename = filename;  // name of the test file
    this.requires = requires;
    this.failReason = failReason;
    this.skipReason = skipReason;
  }

  getAbsolutePath() {
    return fixtures.path('wpt', this.module, this.filename);
  }

  getContent() {
    return fs.readFileSync(this.getAbsolutePath(), 'utf8');
  }

  shouldSkip() {
    return this.failReason || this.skipReason;
  }

  requireIntl() {
    return this.requires.includes('intl');
  }
}

class StatusLoader {
  constructor(path) {
    this.path = path;
    this.loaded = false;
    this.status = null;
    /** @type {WPTTest[]} */
    this.tests = [];
  }

  loadTest(file) {
    let requires = [];
    let failReason;
    let skipReason;
    if (this.status[file]) {
      requires = this.status[file].requires || [];
      failReason = this.status[file].fail;
      skipReason = this.status[file].skip;
    }
    return new WPTTest(this.path, file, requires,
                       failReason, skipReason);
  }

  load() {
    const dir = path.join(__dirname, '..', 'wpt');
    const statusFile = path.join(dir, 'status', `${this.path}.json`);
    const result = JSON.parse(fs.readFileSync(statusFile, 'utf8'));
    this.status = result;

    const list = fs.readdirSync(fixtures.path('wpt', this.path));

    for (const file of list) {
      this.tests.push(this.loadTest(file));
    }
    this.loaded = true;
  }

  get jsTests() {
    return this.tests.filter((test) => test.filename.endsWith('.js'));
  }
}

const PASSED = 1;
const FAILED = 2;
const SKIPPED = 3;

class WPTRunner {
  constructor(path) {
    this.path = path;
    this.resource = new ResourceLoader(path);
    this.sandbox = null;
    this.context = null;

    this.globals = new Map();

    this.status = new StatusLoader(path);
    this.status.load();
    this.tests = new Map(
      this.status.jsTests.map((item) => [item.filename, item])
    );

    this.results = new Map();
    this.inProgress = new Set();
  }

  /**
   * Specify that certain global descriptors from the object
   * should be defined in the vm
   * @param {object} obj
   * @param {string[]} names
   */
  copyGlobalsFromObject(obj, names) {
    for (const name of names) {
      const desc = Object.getOwnPropertyDescriptor(global, name);
      this.globals.set(name, desc);
    }
  }

  /**
   * Specify that certain global descriptors should be defined in the vm
   * @param {string} name
   * @param {object} descriptor
   */
  defineGlobal(name, descriptor) {
    this.globals.set(name, descriptor);
  }

  // TODO(joyeecheung): work with the upstream to port more tests in .html
  // to .js.
  runJsTests() {
    // TODO(joyeecheung): it's still under discussion whether we should leave
    // err.name alone. See https://github.com/nodejs/node/issues/20253
    const internalErrors = require('internal/errors');
    internalErrors.useOriginalName = true;

    let queue = [];

    // If the tests are run as `node test/wpt/test-something.js subset.any.js`,
    // only `subset.any.js` will be run by the runner.
    if (process.argv[2]) {
      const filename = process.argv[2];
      if (!this.tests.has(filename)) {
        throw new Error(`${filename} not found!`);
      }
      queue.push(this.tests.get(filename));
    } else {
      queue = this.buildQueue();
    }

    this.inProgress = new Set(queue.map((item) => item.filename));

    for (const test of queue) {
      const filename = test.filename;
      const content = test.getContent();
      const meta = test.title = this.getMeta(content);

      const absolutePath = test.getAbsolutePath();
      const context = this.generateContext(test.filename);
      const code = this.mergeScripts(meta, content);
      try {
        vm.runInContext(code, context, {
          filename: absolutePath
        });
      } catch (err) {
        this.fail(filename, {
          name: '',
          message: err.message,
          stack: err.stack
        }, 'UNCAUGHT');
        this.inProgress.delete(filename);
      }
    }
    this.tryFinish();
  }

  mock() {
    const resource = this.resource;
    const result = {
      // This is a mock, because at the moment fetch is not implemented
      // in Node.js, but some tests and harness depend on this to pull
      // resources.
      fetch(file) {
        return resource.fetch(file);
      },
      location: {},
      GLOBAL: {
        isWindow() { return false; }
      },
      Object
    };

    return result;
  }

  // Note: this is how our global space for the WPT test should look like
  getSandbox() {
    const result = this.mock();
    for (const [name, desc] of this.globals) {
      Object.defineProperty(result, name, desc);
    }
    return result;
  }

  generateContext(filename) {
    const sandbox = this.sandbox = this.getSandbox();
    const context = this.context = vm.createContext(sandbox);

    const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
    const harness = fs.readFileSync(harnessPath, 'utf8');
    vm.runInContext(harness, context, {
      filename: harnessPath
    });

    sandbox.add_result_callback(
      this.resultCallback.bind(this, filename)
    );
    sandbox.add_completion_callback(
      this.completionCallback.bind(this, filename)
    );
    sandbox.self = sandbox;
    // TODO(joyeecheung): we are not a window - work with the upstream to
    // add a new scope for us.
    sandbox.document = {};  // Pretend we are Window
    return context;
  }

  resultCallback(filename, test) {
    switch (test.status) {
      case 1:
        this.fail(filename, test, 'FAILURE');
        break;
      case 2:
        this.fail(filename, test, 'TIMEOUT');
        break;
      case 3:
        this.fail(filename, test, 'INCOMPLETE');
        break;
      default:
        this.succeed(filename, test);
    }
  }

  completionCallback(filename, tests, harnessStatus) {
    if (harnessStatus.status === 2) {
      assert.fail(`test harness timed out in ${filename}`);
    }
    this.inProgress.delete(filename);
    this.tryFinish();
  }

  tryFinish() {
    if (this.inProgress.size > 0) {
      return;
    }

    this.reportResults();
  }

  reportResults() {
    const unexpectedFailures = [];
    for (const [filename, items] of this.results) {
      const test = this.tests.get(filename);
      let title = test.meta && test.meta.title;
      title = title ? `${filename} : ${title}` : filename;
      console.log(`---- ${title} ----`);
      for (const item of items) {
        switch (item.type) {
          case FAILED: {
            if (test.failReason) {
              console.log(`[EXPECTED_FAILURE] ${item.test.name}`);
            } else {
              console.log(`[UNEXPECTED_FAILURE] ${item.test.name}`);
              unexpectedFailures.push([title, filename, item]);
            }
            break;
          }
          case PASSED: {
            console.log(`[PASSED] ${item.test.name}`);
            break;
          }
          case SKIPPED: {
            console.log(`[SKIPPED] ${item.reason}`);
            break;
          }
        }
      }
    }

    if (unexpectedFailures.length > 0) {
      for (const [title, filename, item] of unexpectedFailures) {
        console.log(`---- ${title} ----`);
        console.log(`[${item.reason}] ${item.test.name}`);
        console.log(item.test.message);
        console.log(item.test.stack);
        const command = `${process.execPath} ${process.execArgv}` +
                        ` ${require.main.filename} ${filename}`;
        console.log(`Command: ${command}\n`);
      }
      assert.fail(`${unexpectedFailures.length} unexpected failures found`);
    }
  }

  addResult(filename, item) {
    const result = this.results.get(filename);
    if (result) {
      result.push(item);
    } else {
      this.results.set(filename, [item]);
    }
  }

  succeed(filename, test) {
    this.addResult(filename, {
      type: PASSED,
      test
    });
  }

  fail(filename, test, reason) {
    this.addResult(filename, {
      type: FAILED,
      test,
      reason
    });
  }

  skip(filename, reason) {
    this.addResult(filename, {
      type: SKIPPED,
      reason
    });
  }

  getMeta(code) {
    const matches = code.match(/\/\/ META: .+/g);
    if (!matches) {
      return {};
    } else {
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
  }

  mergeScripts(meta, content) {
    if (!meta.script) {
      return content;
    }

    // only one script
    let result = '';
    for (const script of meta.script) {
      result += this.resource.fetch(script, false);
    }

    return result + content;
  }

  buildQueue() {
    const queue = [];
    for (const test of this.tests.values()) {
      const filename = test.filename;
      if (test.skipReason) {
        this.skip(filename, test.skipReason);
        continue;
      }

      if (!common.hasIntl && test.requireIntl()) {
        this.skip(filename, 'missing Intl');
        continue;
      }

      queue.push(test);
    }
    return queue;
  }
}

module.exports = {
  harness: harnessMock,
  WPTRunner
};
