'use strict';
// Flags: --expose-internals

/**
 * This test ensures defaultResolve returns the found module format in the
 * return object in the form:
 * { url: <url_value>, format: <'module'|'commonjs'|undefined> };
 */

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');
const fs = require('fs');
const url = require('url');

if (!common.isMainThread) {
  common.skip(
    'test-esm-resolve-type.js: process.chdir is not available in Workers'
  );
}

const assert = require('assert');
const {
  defaultResolve: resolve
} = require('internal/modules/esm/resolve');

const rel = (file) => path.join(tmpdir.path, file);
const previousCwd = process.cwd();
const nmDir = rel('node_modules');
try {
  tmpdir.refresh();
  process.chdir(tmpdir.path);
  /**
   * ensure that resolving by full path does not return the format
   * with the defaultResolver
   */
  [ [ '/es-modules/package-type-module/index.js', undefined ],
    [ '/es-modules/package-type-commonjs/index.js', undefined ],
    [ '/es-modules/package-without-type/index.js', undefined ],
    [ '/es-modules/package-without-pjson/index.js', undefined ],
  ].forEach((testVariant) => {
    const [ testScript, expectedType ] = testVariant;
    const resolvedPath = path.resolve(fixtures.path(testScript));
    const resolveResult = resolve(url.pathToFileURL(resolvedPath));
    assert.strictEqual(resolveResult.format, expectedType);
  });

  /**
   * create a test module and try to resolve it by module name.
   * check the result is as expected
   */

  [ [ 'test-module-mainjs', 'js', 'module', 'module'],
    [ 'test-module-mainmjs', 'mjs', 'module', 'module'],
    [ 'test-module-cjs', 'js', 'commonjs', 'commonjs'],
    [ 'test-module-ne', 'js', undefined, undefined],
  ].forEach((testVariant) => {
    const [ moduleName,
            moduleExtenstion,
            moduleType,
            expectedResolvedType ] = testVariant;
    process.chdir(previousCwd);
    tmpdir.refresh();
    process.chdir(tmpdir.path);
    const createDir = (path) => {
      if (!fs.existsSync(path)) {
        fs.mkdirSync(path);
      }
    };

    const mDir = rel(`node_modules/${moduleName}`);
    const subDir = rel(`node_modules/${moduleName}/subdir`);
    const pkg = rel(`node_modules/${moduleName}/package.json`);
    const script = rel(`node_modules/${moduleName}/subdir/mainfile.${moduleExtenstion}`);

    createDir(nmDir);
    createDir(mDir);
    createDir(subDir);
    const pkgJsonContent = {
      ...(moduleType !== undefined) && { type: moduleType },
      main: `subdir/mainfile.${moduleExtenstion}`
    };
    fs.writeFileSync(pkg, JSON.stringify(pkgJsonContent));
    fs.writeFileSync(script,
                     'export function esm-resolve-tester() {return 42}');

    const resolveResult = resolve(`${moduleName}`);
    assert.strictEqual(resolveResult.format, expectedResolvedType);

    fs.rmSync(nmDir, { recursive: true, force: true });
  });

  // Helpers
  const createDir = (path) => {
    if (!fs.existsSync(path)) {
      fs.mkdirSync(path);
    }
  };

  function testDualPackageWithJsMainScriptAndModuleType() {
    // Create a dummy dual package
    //
    /**
     * this creates following directory structure:
     *
     * ./node_modules:
     *   |-> my-dual-package
     *       |-> es
     *           |-> index.js
     *           |-> package.json [2]
     *       |-> lib
     *           |-> index.js
     *       |->package.json [1]
     *
     * [1] - main package.json of the package
     *     - it contains:
     *             - type: 'commonjs'
     *             - main: 'lib/mainfile.js'
     *             - conditional exports for 'require' (lib/index.js) and
     *                                       'import' (es/index.js)
     * [2] - package.json add-on for the import case
     *     - it only contains:
     *             - type: 'module'
     *
     * in case the package is consumed as an ESM by importing it:
     *    import * as my-package from 'my-dual-package'
     * it will cause the resolve method to return:
     *  {
     *     url: '<base_path>/node_modules/my-dual-package/es/index.js',
     *     format: 'module'
     *  }
     *
     *  following testcase ensures that resolve works correctly in this case
     *  returning the information as specified above. Source for 'url' value
     *  is [1], source for 'format' value is [2]
     */

    const moduleName = 'my-dual-package';

    const mDir = rel(`node_modules/${moduleName}`);
    const esSubDir = rel(`node_modules/${moduleName}/es`);
    const cjsSubDir = rel(`node_modules/${moduleName}/lib`);
    const pkg = rel(`node_modules/${moduleName}/package.json`);
    const esmPkg = rel(`node_modules/${moduleName}/es/package.json`);
    const esScript = rel(`node_modules/${moduleName}/es/index.js`);
    const cjsScript = rel(`node_modules/${moduleName}/lib/index.js`);

    createDir(nmDir);
    createDir(mDir);
    createDir(esSubDir);
    createDir(cjsSubDir);

    const mainPkgJsonContent = {
      type: 'commonjs',
      main: 'lib/index.js',
      exports: {
        '.': {
          'require': './lib/index.js',
          'import': './es/index.js'
        },
        './package.json': './package.json',
      }
    };
    const esmPkgJsonContent = {
      type: 'module'
    };

    fs.writeFileSync(pkg, JSON.stringify(mainPkgJsonContent));
    fs.writeFileSync(esmPkg, JSON.stringify(esmPkgJsonContent));
    fs.writeFileSync(esScript,
                     'export function esm-resolve-tester() {return 42}');
    fs.writeFileSync(cjsScript,
                     `module.exports = { 
                        esm-resolve-tester: () => {return 42}}`
    );

    // test the resolve
    const resolveResult = resolve(`${moduleName}`);
    assert.strictEqual(resolveResult.format, 'module');
    assert.ok(resolveResult.url.includes('my-dual-package/es/index.js'));
  }

  function testDualPackageWithMjsMainScriptAndCJSType() {

    // Additional test for following scenario
    /**
     * this creates following directory structure:
     *
     * ./node_modules:
     *   |-> dual-mjs-pjson
     *       |-> subdir
     *           |-> index.mjs [3]
     *           |-> package.json [2]
     *           |-> index.js
     *       |->package.json [1]
     *
     * [1] - main package.json of the package
     *     - it contains:
     *             - type: 'commonjs'
     *             - main: 'subdir/index.js'
     *             - conditional exports for 'require' (subdir/index.js) and
     *                                       'import' (subdir/index.mjs)
     * [2] - package.json add-on for the import case
     *     - it only contains:
     *             - type: 'commonjs'
     * [3] - main script for the `import` case
     *
     * in case the package is consumed as an ESM by importing it:
     *    import * as my-package from 'dual-mjs-pjson'
     * it will cause the resolve method to return:
     *  {
     *     url: '<base_path>/node_modules/dual-mjs-pjson/subdir/index.mjs',
     *     format: 'module'
     *  }
     *
     *  following testcase ensures that resolve works correctly in this case
     *  returning the information as specified above. Source for 'url' value
     *  is [1], source for 'format' value is the file extension of [3]
     */
    const moduleName = 'dual-mjs-pjson';

    const mDir = rel(`node_modules/${moduleName}`);
    const subDir = rel(`node_modules/${moduleName}/subdir`);
    const pkg = rel(`node_modules/${moduleName}/package.json`);
    const subdirPkg = rel(`node_modules/${moduleName}/subdir/package.json`);
    const esScript = rel(`node_modules/${moduleName}/subdir/index.mjs`);
    const cjsScript = rel(`node_modules/${moduleName}/subdir/index.js`);

    createDir(nmDir);
    createDir(mDir);
    createDir(subDir);

    const mainPkgJsonContent = {
      type: 'commonjs',
      main: 'lib/index.js',
      exports: {
        '.': {
          'require': './subdir/index.js',
          'import': './subdir/index.mjs'
        },
        './package.json': './package.json',
      }
    };
    const subdirPkgJsonContent = {
      type: 'commonjs'
    };

    fs.writeFileSync(pkg, JSON.stringify(mainPkgJsonContent));
    fs.writeFileSync(subdirPkg, JSON.stringify(subdirPkgJsonContent));
    fs.writeFileSync(esScript,
                     'export function esm-resolve-tester() {return 42}');
    fs.writeFileSync(cjsScript,
                     `module.exports = { 
                      esm-resolve-tester: () => {return 42}}`
    );

    // test the resolve
    const resolveResult = resolve(`${moduleName}`);
    assert.strictEqual(resolveResult.format, 'module');
    assert.ok(resolveResult.url.includes(`${moduleName}/subdir/index.mjs`));
  }

  testDualPackageWithJsMainScriptAndModuleType();
  testDualPackageWithMjsMainScriptAndCJSType();

} finally {
  process.chdir(previousCwd);
  fs.rmSync(nmDir, { recursive: true, force: true });
}
