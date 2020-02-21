/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const vm = require('vm');
const { inspect } = require('util');

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

  /**
   * Load a resource in test/fixtures/wpt specified with a URL
   * @param {string} from the path of the file loading this resource,
   *                      relative to thw WPT folder.
   * @param {string} url the url of the resource being loaded.
   * @param {boolean} asPromise if true, return the resource in a
   *                            pseudo-Response object.
   */
  read(from, url, asFetch = true) {
    // We need to patch this to load the WebIDL parser
    url = url.replace(
      '/resources/WebIDLParser.js',
      '/resources/webidl2/lib/webidl2.js'
    );
    const base = path.dirname(from);
    const file = url.startsWith('/') ?
      fixtures.path('wpt', url) :
      fixtures.path('wpt', base, url);
    if (asFetch) {
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
        this.exactMatch[key] = new StatusRule(key, rules[key]);
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

class WPTTest {
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
    /** @type {WPTTest[]} */
    this.tests = [];
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
      this.tests.push(new WPTTest(this.path, relativePath, match));
    }
    this.loaded = true;
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
      this.status.tests.map((item) => [item.filename, item])
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
      const desc = Object.getOwnPropertyDescriptor(obj, name);
      if (!desc) {
        assert.fail(`${name} does not exist on the object`);
      }
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
      const context = this.generateContext(test);
      const relativePath = test.getRelativePath();
      const code = this.mergeScripts(relativePath, meta, content);
      try {
        vm.runInContext(code, context, {
          filename: absolutePath
        });
      } catch (err) {
        this.fail(filename, {
          name: '',
          message: err.message,
          stack: inspect(err)
        }, 'UNCAUGHT');
        this.inProgress.delete(filename);
      }
    }
    this.tryFinish();
  }

  mock(testfile) {
    const resource = this.resource;
    const result = {
      // This is a mock, because at the moment fetch is not implemented
      // in Node.js, but some tests and harness depend on this to pull
      // resources.
      fetch(file) {
        return resource.read(testfile, file, true);
      },
      GLOBAL: {
        isWindow() { return false; }
      },
      Object
    };

    return result;
  }

  // Note: this is how our global space for the WPT test should look like
  getSandbox(filename) {
    const result = this.mock(filename);
    for (const [name, desc] of this.globals) {
      Object.defineProperty(result, name, desc);
    }
    return result;
  }

  generateContext(test) {
    const filename = test.filename;
    const sandbox = this.sandbox = this.getSandbox(test.getRelativePath());
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
            if (test.failReasons.length) {
              console.log(`[EXPECTED_FAILURE] ${item.test.name}`);
              console.log(test.failReasons.join('; '));
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

  skip(filename, reasons) {
    this.addResult(filename, {
      type: SKIPPED,
      reason: reasons.join('; ')
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

  mergeScripts(base, meta, content) {
    if (!meta.script) {
      return content;
    }

    // only one script
    let result = '';
    for (const script of meta.script) {
      result += this.resource.read(base, script, false);
    }

    return result + content;
  }

  buildQueue() {
    const queue = [];
    for (const test of this.tests.values()) {
      const filename = test.filename;
      if (test.skipReasons.length > 0) {
        this.skip(filename, test.skipReasons);
        continue;
      }

      const lackingIntl = intlRequirements.isLacking(test.requires);
      if (lackingIntl) {
        this.skip(filename, [ `requires ${lackingIntl}` ]);
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
