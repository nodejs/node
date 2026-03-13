'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { builtinModules } = require('module');
const publicUnprefixedModules = builtinModules.filter((lib) => !lib.startsWith('_') && !lib.startsWith('node:'));

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

// We have to change the directory to ../fixtures before requiring repl
// in order to make the tests for completion of node_modules work properly
// since repl modifies module.paths.
process.chdir(fixtures.fixturesDir);

const repl = require('repl');
const { startNewREPLServer } = require('../common/repl');

const { replServer, input } = startNewREPLServer();

// Tab complete provides built in libs for import()
replServer.complete('import(\'', common.mustSucceed((data) => {
  publicUnprefixedModules.forEach((lib) => {
    assert(
      data[0].includes(lib) && data[0].includes(`node:${lib}`),
      `${lib} not found`,
    );
  });
  const newModule = 'foobar';
  assert(!builtinModules.includes(newModule));
  repl.builtinModules.push(newModule);
  replServer.complete('import(\'', common.mustSucceed(([modules]) => {
    assert.strictEqual(data[0].length + 1, modules.length);
    assert(modules.includes(newModule) &&
      !modules.includes(`node:${newModule}`));
  }));
}));

replServer.complete("import\t( 'n", common.mustSucceed((data) => {
  assert.strictEqual(data.length, 2);
  assert.strictEqual(data[1], 'n');
  const completions = data[0];
  // import(...) completions include `node:` URL modules:
  let lastIndex = -1;

  publicUnprefixedModules.forEach((lib, index) => {
    lastIndex = completions.indexOf(`node:${lib}`);
    assert.notStrictEqual(lastIndex, -1);
  });
  assert.strictEqual(completions[lastIndex + 1], '');
  // There is only one Node.js module that starts with n:
  assert.strictEqual(completions[lastIndex + 2], 'net');
  assert.strictEqual(completions[lastIndex + 3], '');
  // It's possible to pick up non-core modules too
  completions.slice(lastIndex + 4).forEach((completion) => {
    assert.match(completion, /^n/);
  });
}));

{
  const expected = ['@nodejsscope', '@nodejsscope/'];
  // Import calls should handle all types of quotation marks.
  for (const quotationMark of ["'", '"', '`']) {
    input.run(['.clear']);
    replServer.complete('import(`@nodejs', common.mustSucceed((data) => {
      assert.deepStrictEqual(data, [expected, '@nodejs']);
    }));

    input.run(['.clear']);
    // Completions should not be greedy in case the quotation ends.
    replServer.complete(`import(${quotationMark}@nodejsscope${quotationMark}`, common.mustSucceed((data) => {
      assert.deepStrictEqual(data, [[], undefined]);
    }));
  }
}

{
  input.run(['.clear']);
  // Completions should find modules and handle whitespace after the opening
  // bracket.
  replServer.complete('import \t("no_ind', common.mustSucceed((data) => {
    assert.deepStrictEqual(data, [['no_index', 'no_index/'], 'no_ind']);
  }));
}

// Test tab completion for import() relative to the current directory
{
  input.run(['.clear']);

  const cwd = process.cwd();
  process.chdir(__dirname);

  ['import(\'.', 'import(".'].forEach((input) => {
    replServer.complete(input, common.mustSucceed((data) => {
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], '.');
      assert.strictEqual(data[0].length, 2);
      assert.ok(data[0].includes('./'));
      assert.ok(data[0].includes('../'));
    }));
  });

  ['import(\'..', 'import("..'].forEach((input) => {
    replServer.complete(input, common.mustSucceed((data) => {
      assert.deepStrictEqual(data, [['../'], '..']);
    }));
  });

  ['./', './test-'].forEach((path) => {
    [`import('${path}`, `import("${path}`].forEach((input) => {
      replServer.complete(input, common.mustSucceed((data) => {
        assert.strictEqual(data.length, 2);
        assert.strictEqual(data[1], path);
        assert.ok(data[0].includes('./test-repl-tab-complete.js'));
      }));
    });
  });

  ['../parallel/', '../parallel/test-'].forEach((path) => {
    [`import('${path}`, `import("${path}`].forEach((input) => {
      replServer.complete(input, common.mustSucceed((data) => {
        assert.strictEqual(data.length, 2);
        assert.strictEqual(data[1], path);
        assert.ok(data[0].includes('../parallel/test-repl-tab-complete.js'));
      }));
    });
  });

  {
    const path = '../fixtures/repl-folder-extensions/f';
    replServer.complete(`import('${path}`, common.mustSucceed((data) => {
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], path);
      assert.ok(data[0].includes(
        '../fixtures/repl-folder-extensions/foo.js/'));
    }));
  }

  process.chdir(cwd);
}
