import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawn } from 'node:child_process';
import { describe, it } from 'node:test';
import assert from 'node:assert';

describe('Module syntax detection', { concurrency: !process.env.TEST_PARALLEL }, () => {
  describe('string input', { concurrency: !process.env.TEST_PARALLEL }, () => {
    it('permits ESM syntax in --eval input without requiring --input-type=module', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--eval',
        'import { version } from "node:process"; console.log(version);',
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, `${process.version}\n`);
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    // ESM is unsupported for --print via --input-type=module

    it('permits ESM syntax in STDIN input without requiring --input-type=module', async () => {
      const child = spawn(process.execPath, []);
      child.stdin.end('console.log(typeof import.meta.resolve)');

      assert.match((await child.stdout.toArray()).toString(), /^function\r?\n$/);
    });

    it('should be overridden by --input-type', async () => {
      const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
        '--input-type=commonjs',
        '--eval',
        'import.meta.url',
      ]);

      assert.match(stderr, /SyntaxError: Cannot use 'import\.meta' outside a module/);
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });

    it('should not switch to module if code is parsable as script', async () => {
      const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
        '--eval',
        'let __filename,__dirname,require,module,exports;this.a',
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    it('does not trigger detection via source code `eval()`', async () => {
      const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
        '--eval',
        'eval("import \'nonexistent\';");',
      ]);

      assert.match(stderr, /SyntaxError: Cannot use import statement outside a module/);
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });
  });

  describe('.js file input in a typeless package', { concurrency: !process.env.TEST_PARALLEL }, () => {
    for (const { testName, entryPath } of [
      {
        testName: 'permits CommonJS syntax in a .js entry point',
        entryPath: fixtures.path('es-modules/package-without-type/commonjs.js'),
      },
      {
        testName: 'permits ESM syntax in a .js entry point',
        entryPath: fixtures.path('es-modules/package-without-type/module.js'),
      },
      {
        testName: 'permits CommonJS syntax in a .js file imported by a CommonJS entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-commonjs.cjs'),
      },
      {
        testName: 'permits ESM syntax in a .js file imported by a CommonJS entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-esm.js'),
      },
      {
        testName: 'permits CommonJS syntax in a .js file imported by an ESM entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-commonjs.mjs'),
      },
      {
        testName: 'permits ESM syntax in a .js file imported by an ESM entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-esm.mjs'),
      },
    ]) {
      it(testName, async () => {
        const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
          '--no-warnings',
          entryPath,
        ]);

        assert.strictEqual(stderr, '');
        assert.strictEqual(stdout, 'executed\n');
        assert.strictEqual(code, 0);
        assert.strictEqual(signal, null);
      });
    }
  });

  describe('extensionless file input in a typeless package', { concurrency: !process.env.TEST_PARALLEL }, () => {
    for (const { testName, entryPath } of [
      {
        testName: 'permits CommonJS syntax in an extensionless entry point',
        entryPath: fixtures.path('es-modules/package-without-type/noext-cjs'),
      },
      {
        testName: 'permits ESM syntax in an extensionless entry point',
        entryPath: fixtures.path('es-modules/package-without-type/noext-esm'),
      },
      {
        testName: 'permits CommonJS syntax in an extensionless file imported by a CommonJS entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-noext-cjs.js'),
      },
      {
        testName: 'permits ESM syntax in an extensionless file imported by a CommonJS entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-noext-esm.js'),
      },
      {
        testName: 'permits CommonJS syntax in an extensionless file imported by an ESM entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-noext-cjs.mjs'),
      },
      {
        testName: 'permits ESM syntax in an extensionless file imported by an ESM entry point',
        entryPath: fixtures.path('es-modules/package-without-type/imports-noext-esm.mjs'),
      },
    ]) {
      it(testName, async () => {
        const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
          '--no-warnings',
          entryPath,
        ]);

        assert.strictEqual(stderr, '');
        assert.strictEqual(stdout, 'executed\n');
        assert.strictEqual(code, 0);
        assert.strictEqual(signal, null);
      });
    }

    it('should not hint wrong format in resolve hook', async () => {
      let writeSync;
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--no-warnings',
        '--loader',
        `data:text/javascript,import { writeSync } from "node:fs"; export ${encodeURIComponent(
          async function resolve(s, c, next) {
            const result = await next(s, c);
            writeSync(1, result.format + '\n');
            return result;
          }
        )}`,
        fixtures.path('es-modules/package-without-type/noext-esm'),
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, 'null\nexecuted\n');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);

    });
  });

  describe('file input in a "type": "commonjs" package', { concurrency: !process.env.TEST_PARALLEL }, () => {
    for (const { testName, entryPath } of [
      {
        testName: 'disallows ESM syntax in a .js entry point',
        entryPath: fixtures.path('es-modules/package-type-commonjs/module.js'),
      },
      {
        testName: 'disallows ESM syntax in a .js file imported by a CommonJS entry point',
        entryPath: fixtures.path('es-modules/package-type-commonjs/imports-esm.js'),
      },
      {
        testName: 'disallows ESM syntax in a .js file imported by an ESM entry point',
        entryPath: fixtures.path('es-modules/package-type-commonjs/imports-esm.mjs'),
      },
    ]) {
      it(testName, async () => {
        const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
          entryPath,
        ]);

        assert.match(stderr, /SyntaxError: Unexpected token 'export'/);
        assert.strictEqual(stdout, '');
        assert.strictEqual(code, 1);
        assert.strictEqual(signal, null);
      });
    }
  });

  describe('file input in a "type": "module" package', { concurrency: !process.env.TEST_PARALLEL }, () => {
    for (const { testName, entryPath } of [
      {
        testName: 'disallows CommonJS syntax in a .js entry point',
        entryPath: fixtures.path('es-modules/package-type-module/cjs.js'),
      },
      {
        testName: 'disallows CommonJS syntax in a .js file imported by a CommonJS entry point',
        entryPath: fixtures.path('es-modules/package-type-module/imports-commonjs.cjs'),
      },
      {
        testName: 'disallows CommonJS syntax in a .js file imported by an ESM entry point',
        entryPath: fixtures.path('es-modules/package-type-module/imports-commonjs.mjs'),
      },
    ]) {
      it(testName, async () => {
        const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
          entryPath,
        ]);

        assert.match(stderr, /ReferenceError: module is not defined in ES module scope/);
        assert.strictEqual(stdout, '');
        assert.strictEqual(code, 1);
        assert.strictEqual(signal, null);
      });
    }
  });

  // https://github.com/nodejs/node/issues/50917
  describe('syntax that errors in CommonJS but works in ESM', { concurrency: !process.env.TEST_PARALLEL }, () => {
    it('permits top-level `await`', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--eval',
        'await Promise.resolve(); console.log("executed");',
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, 'executed\n');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    it('reports unfinished top-level `await`', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--no-warnings',
        fixtures.path('es-modules/tla/unresolved.js'),
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 13);
      assert.strictEqual(signal, null);
    });

    it('permits top-level `await` above import/export syntax', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--eval',
        'await Promise.resolve(); import "node:os"; console.log("executed");',
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, 'executed\n');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    it('still throws on `await` in an ordinary sync function', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--eval',
        'function fn() { await Promise.resolve(); } fn();',
      ]);

      assert.match(stderr, /SyntaxError: await is only valid in async function/);
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });

    it('throws on undefined `require` when top-level `await` triggers ESM parsing', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--eval',
        'const fs = require("node:fs"); await Promise.resolve();',
      ]);

      assert.match(
        stderr,
        /ReferenceError: Cannot determine intended module format because both 'require' and top-level await are present\. If the code is intended to be CommonJS, wrap await in an async function\. If the code is intended to be an ES module, replace require\(\) with import\./
      );
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });

    it('permits declaration of CommonJS module variables', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--no-warnings',
        fixtures.path('es-modules/package-without-type/commonjs-wrapper-variables.js'),
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, 'exports require module __filename __dirname\n');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    it('permits declaration of CommonJS module variables above import/export', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--eval',
        'const module = 3; import "node:os"; console.log("executed");',
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, 'executed\n');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    it('still throws on double `const` declaration not at the top level', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--eval',
        'function fn() { const require = 1; const require = 2; } fn();',
      ]);

      assert.match(stderr, /SyntaxError: Identifier 'require' has already been declared/);
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });
  });

  describe('warn about typeless packages for .js files with ESM syntax', { concurrency: true }, () => {
    for (const { testName, entryPath } of [
      {
        testName: 'warns for ESM syntax in a .js entry point in a typeless package',
        entryPath: fixtures.path('es-modules/package-without-type/module.js'),
      },
      {
        testName: 'warns for ESM syntax in a .js file imported by a CommonJS entry point in a typeless package',
        entryPath: fixtures.path('es-modules/package-without-type/imports-esm.js'),
      },
      {
        testName: 'warns for ESM syntax in a .js file imported by an ESM entry point in a typeless package',
        entryPath: fixtures.path('es-modules/package-without-type/imports-esm.mjs'),
      },
    ]) {
      it(testName, async () => {
        const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
          entryPath,
        ]);

        assert.match(stderr, /MODULE_TYPELESS_PACKAGE_JSON/);
        assert.strictEqual(stdout, 'executed\n');
        assert.strictEqual(code, 0);
        assert.strictEqual(signal, null);
      });
    }

    it('does not warn when there are no package.json', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        fixtures.path('es-modules/loose.js'),
      ]);

      assert.strictEqual(stderr, '');
      assert.strictEqual(stdout, 'executed\n');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });


    it('warns only once for a package.json that affects multiple files', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        fixtures.path('es-modules/package-without-type/detected-as-esm.js'),
      ]);

      assert.match(stderr, /MODULE_TYPELESS_PACKAGE_JSON/);
      assert.strictEqual(stderr.match(/MODULE_TYPELESS_PACKAGE_JSON/g).length, 1);
      assert.strictEqual(stdout, 'executed\nexecuted\n');
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    it('can be disabled via --no-experimental-detect-module', async () => {
      const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
        '--no-experimental-detect-module',
        fixtures.path('es-modules/package-without-type/module.js'),
      ]);

      assert.match(stderr, /SyntaxError: Unexpected token 'export'/);
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    });
  });
});

// Checks the error caught during module detection does not trigger abort when
// `--abort-on-uncaught-exception` is passed in (as that's a caught internal error).
// Ref: https://github.com/nodejs/node/issues/50878
describe('Wrapping a `require` of an ES module while using `--abort-on-uncaught-exception`', () => {
  it('should work', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--abort-on-uncaught-exception',
      '--no-warnings',
      '--eval',
      'require("./package-type-module/esm.js")',
    ], {
      cwd: fixtures.path('es-modules'),
    });

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});

describe('when working with Worker threads', () => {
  it('should support sloppy scripts that declare CJS "global-like" variables', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--eval',
      'new worker_threads.Worker("let __filename,__dirname,require,module,exports;this.a",{eval:true})',
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});

describe('cjs & esm ambiguous syntax case', () => {
  it('should throw an ambiguous syntax error when using top-level await with require', async () => {
    const { stderr, code, signal } = await spawnPromisified(
      process.execPath,
      [
        '--input-type=module',
        '--eval',
        `await 1;\nconst fs = require('fs');`,
      ]
    );

    assert.match(
      stderr,
      /ReferenceError: Cannot determine intended module format because both 'require' and top-level await are present\. If the code is intended to be CommonJS, wrap await in an async function\. If the code is intended to be an ES module, replace require\(\) with import\./
    );

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should throw an ambiguous syntax error when using top-level await with exports', async () => {
    const { stderr, code, signal } = await spawnPromisified(
      process.execPath,
      [
        '--eval',
        `exports.foo = 'bar';\nawait 1;`,
      ]
    );

    assert.match(
      stderr,
      /ReferenceError: Cannot determine intended module format because both 'exports' and top-level await are present\. If the code is intended to be CommonJS, wrap await in an async function\. If the code is intended to be an ES module, use export instead of module\.exports\/exports\./
    );

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should throw an ambiguous syntax error when using top-level await with __filename', async () => {
    const { stderr, code, signal } = await spawnPromisified(
      process.execPath,
      [
        '--eval',
        `console.log(__filename);\nawait 1;`,
      ]
    );

    assert.match(
      stderr,
      /ReferenceError: Cannot determine intended module format because both '__filename' and top-level await are present\. If the code is intended to be CommonJS, wrap await in an async function\. If the code is intended to be an ES module, use import\.meta\.filename instead\./
    );

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should throw an ambiguous syntax error when using top-level await with __dirname', async () => {
    const { stderr, code, signal } = await spawnPromisified(
      process.execPath,
      [
        '--eval',
        `console.log(__dirname);\nawait 1;`,
      ]
    );

    assert.match(
      stderr,
      /ReferenceError: Cannot determine intended module format because both '__dirname' and top-level await are present\. If the code is intended to be CommonJS, wrap await in an async function\. If the code is intended to be an ES module, use import\.meta\.dirname instead\./
    );

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });
});
