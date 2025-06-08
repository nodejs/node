'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { builtinModules } = require('module');
const publicModules = builtinModules.filter((lib) => !lib.startsWith('_'));

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

// We have to change the directory to ../fixtures before requiring repl
// in order to make the tests for completion of node_modules work properly
// since repl modifies module.paths.
process.chdir(fixtures.fixturesDir);

const repl = require('repl');

function prepareREPL() {
  const replServer = repl.start({
    prompt: '',
    input: new ArrayStream(),
    output: process.stdout,
    allowBlockingCompletions: true,
  });

  // Some errors are passed to the domain, but do not callback
  replServer._domain.on('error', assert.ifError);

  return replServer;
}

// Tab completion on require on builtin modules works
{
  const replServer = prepareREPL();

  replServer.complete(
    "require('",
    common.mustCall(async function(error, data) {
      assert.strictEqual(error, null);
      publicModules.forEach((lib) => {
        assert(
          data[0].includes(lib) &&
            (lib.startsWith('node:') || data[0].includes(`node:${lib}`)),
          `${lib} not found`
        );
      });
      const newModule = 'foobar';
      assert(!builtinModules.includes(newModule));
      repl.builtinModules.push(newModule);
      replServer.complete(
        "require('",
        common.mustCall((_, [modules]) => {
          assert.strictEqual(data[0].length + 1, modules.length);
          assert(modules.includes(newModule));
        })
      );
    })
  );
}

// Tab completion on require on builtin modules works (with extra spaces and "n" prefix)
{
  const replServer = prepareREPL();

  replServer.complete(
    "require\t( 'n",
    common.mustCall(function(error, data) {
      assert.strictEqual(error, null);
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], 'n');
      // require(...) completions include `node:`-prefixed modules:
      let lastIndex = -1;

      publicModules
        .filter((lib) => !lib.startsWith('node:'))
        .forEach((lib, index) => {
          lastIndex = data[0].indexOf(`node:${lib}`);
          assert.notStrictEqual(lastIndex, -1);
        });
      assert.strictEqual(data[0][lastIndex + 1], '');
      // There is only one Node.js module that starts with n:
      assert.strictEqual(data[0][lastIndex + 2], 'net');
      assert.strictEqual(data[0][lastIndex + 3], '');
      // It's possible to pick up non-core modules too
      data[0].slice(lastIndex + 4).forEach((completion) => {
        assert.match(completion, /^n/);
      });
    })
  );
}

// Tab completion on require on external modules works
{
  const expected = ['@nodejsscope', '@nodejsscope/'];

  const replServer = prepareREPL();

  // Require calls should handle all types of quotation marks.
  for (const quotationMark of ["'", '"', '`']) {
    replServer.complete(
      'require(`@nodejs',
      common.mustCall((err, data) => {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(data, [expected, '@nodejs']);
      })
    );

    // Completions should not be greedy in case the quotation ends.
    const input = `require(${quotationMark}@nodejsscope${quotationMark}`;
    replServer.complete(
      input,
      common.mustCall((err, data) => {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(data, [[], undefined]);
      })
    );
  }
}

{
  // Completions should find modules and handle whitespace after the opening bracket.
  const replServer = prepareREPL();

  replServer.complete(
    'require \t("no_ind',
    common.mustCall((err, data) => {
      assert.strictEqual(err, null);
      assert.deepStrictEqual(data, [['no_index', 'no_index/'], 'no_ind']);
    })
  );
}

// Test tab completion for require() relative to the current directory
{
  const replServer = prepareREPL();

  const cwd = process.cwd();
  process.chdir(__dirname);

  ["require('.", 'require(".'].forEach((input) => {
    replServer.complete(
      input,
      common.mustCall((err, data) => {
        assert.strictEqual(err, null);
        assert.strictEqual(data.length, 2);
        assert.strictEqual(data[1], '.');
        assert.strictEqual(data[0].length, 2);
        assert.ok(data[0].includes('./'));
        assert.ok(data[0].includes('../'));
      })
    );
  });

  ["require('..", 'require("..'].forEach((input) => {
    replServer.complete(
      input,
      common.mustCall((err, data) => {
        assert.strictEqual(err, null);
        assert.deepStrictEqual(data, [['../'], '..']);
      })
    );
  });

  ['./', './test-'].forEach((path) => {
    [`require('${path}`, `require("${path}`].forEach((input) => {
      replServer.complete(
        input,
        common.mustCall((err, data) => {
          assert.strictEqual(err, null);
          assert.strictEqual(data.length, 2);
          assert.strictEqual(data[1], path);
          assert.ok(data[0].includes('./test-repl-tab-complete'));
        })
      );
    });
  });

  ['../parallel/', '../parallel/test-'].forEach((path) => {
    [`require('${path}`, `require("${path}`].forEach((input) => {
      replServer.complete(
        input,
        common.mustCall((err, data) => {
          assert.strictEqual(err, null);
          assert.strictEqual(data.length, 2);
          assert.strictEqual(data[1], path);
          assert.ok(data[0].includes('../parallel/test-repl-tab-complete'));
        })
      );
    });
  });

  {
    const path = '../fixtures/repl-folder-extensions/f';
    replServer.complete(
      `require('${path}`,
      common.mustSucceed((data) => {
        assert.strictEqual(data.length, 2);
        assert.strictEqual(data[1], path);
        assert.ok(
          data[0].includes('../fixtures/repl-folder-extensions/foo.js')
        );
      })
    );
  }

  process.chdir(cwd);
}
