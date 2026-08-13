'use strict';

const common = require('../common');
common.requireNoPackageJSONAbove();

const { it, describe } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const envSuffix = common.isWindows ? '-windows' : '';

describe('node --run [command]', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('returns error on non-existent file', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'test'],
      { cwd: __dirname },
    );
    assert.match(child.stderr, /Can't find package\.json[\s\S]*/);
    // Ensure we show the path that starting path for the search
    assert(child.stderr.includes(__dirname));
    assert.strictEqual(child.stdout, '');
    assert.strictEqual(child.code, 1);
  });

  it('runs a valid command', async () => {
    // Run a script that just log `no test specified`
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'test', '--no-warnings'],
      { cwd: fixtures.path('run-script') },
    );
    assert.match(child.stdout, /Error: no test specified/);
    assert.strictEqual(child.code, 1);
  });

  it('recognizes cmd.exe case-insensitively', {
    skip: !common.isWindows,
  }, async () => {
    const env = { ...process.env };
    const comspecKey = Object.keys(env)
      .find((key) => key.toLowerCase() === 'comspec');
    assert.notStrictEqual(comspecKey, undefined);
    const comspec = env[comspecKey];
    assert.match(comspec, /cmd\.exe$/i);
    delete env[comspecKey];
    env.ComSpec = comspec.replace(/cmd\.exe$/i, 'CMD.EXE');

    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'pwd-windows'],
      { cwd: fixtures.path('run-script'), env },
    );
    assert.strictEqual(child.stdout.trim(), fixtures.path('run-script'));
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('adds node_modules/.bin to path', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', `ada${envSuffix}`],
      { cwd: fixtures.path('run-script') },
    );
    assert.match(child.stdout, /06062023/);
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('chdirs into package directory', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', `pwd${envSuffix}`],
      { cwd: fixtures.path('run-script/sub-directory') },
    );
    assert.strictEqual(child.stdout.trim(), fixtures.path('run-script'));
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('includes actionable info when possible', async () => {
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'missing'],
        { cwd: fixtures.path('run-script') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/package.json')));
      assert(child.stderr.includes('no test specified'));
      assert.strictEqual(child.code, 1);
    }
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'test'],
        { cwd: fixtures.path('run-script/missing-scripts') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/missing-scripts/package.json')));
      assert.strictEqual(child.code, 1);
    }
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'test'],
        { cwd: fixtures.path('run-script/invalid-json') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/invalid-json/package.json')));
      assert.strictEqual(child.code, 1);
    }
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'array'],
        { cwd: fixtures.path('run-script/invalid-schema') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/invalid-schema/package.json')));
      assert.strictEqual(child.code, 1);
    }
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'boolean'],
        { cwd: fixtures.path('run-script/invalid-schema') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/invalid-schema/package.json')));
      assert.strictEqual(child.code, 1);
    }
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'null'],
        { cwd: fixtures.path('run-script/invalid-schema') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/invalid-schema/package.json')));
      assert.strictEqual(child.code, 1);
    }
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'number'],
        { cwd: fixtures.path('run-script/invalid-schema') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/invalid-schema/package.json')));
      assert.strictEqual(child.code, 1);
    }
    {
      const child = await common.spawnPromisified(
        process.execPath,
        [ '--run', 'object'],
        { cwd: fixtures.path('run-script/invalid-schema') },
      );
      assert.strictEqual(child.stdout, '');
      assert(child.stderr.includes(fixtures.path('run-script/invalid-schema/package.json')));
      assert.strictEqual(child.code, 1);
    }
  });

  it('appends positional arguments', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', `positional-args${envSuffix}`, '--', '--help "hello world test"', 'A', 'B', 'C', 'I think therefore I\'m'],
      { cwd: fixtures.path('run-script') },
    );
    if (common.isWindows) {
      assert.match(child.stdout, /Arguments: '--help ""hello world test"" A B C I think therefore I'm'/);
    } else {
      assert.match(child.stdout, /Arguments: '--help "hello world test" A B C I think therefore I'm'/);
    }
    assert.match(child.stdout, /The total number of arguments is: 5/);
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should set PATH environment variable with paths appended with node_modules/.bin', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', `path-env${envSuffix}`],
      { cwd: fixtures.path('run-script/sub-directory') },
    );
    assert.ok(child.stdout.includes(fixtures.path('run-script/node_modules/.bin')));

    // The following test ensures that we do not add paths that does not contain
    // "node_modules/.bin"
    assert.ok(!child.stdout.includes(fixtures.path('node_modules/.bin')));

    // The following test ensures that we add paths that contains "node_modules/.bin"
    assert.ok(child.stdout.includes(fixtures.path('run-script/sub-directory/node_modules/.bin')));

    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should set special environment variables', async () => {
    const scriptName = `special-env-variables${envSuffix}`;
    const packageJsonPath = fixtures.path('run-script/package.json');
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', scriptName],
      { cwd: fixtures.path('run-script') },
    );
    assert.ok(child.stdout.includes(scriptName));
    assert.ok(child.stdout.includes(packageJsonPath));
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('will search parent directories for a package.json file', async () => {
    const packageJsonPath = fixtures.path('run-script/package.json');
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', `special-env-variables${envSuffix}`],
      { cwd: fixtures.path('run-script/sub-directory') },
    );
    assert.ok(child.stdout.includes(packageJsonPath));
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('handles package paths outside the active Windows code page',
     { skip: !common.isWindows }, async () => {
       tmpdir.refresh();

       const projectDir = path.join(tmpdir.path, 'node-run-\u{20BB7}');
       const packageJsonPath = path.join(projectDir, 'package.json');
       const nodeModulesBin = path.join(projectDir, 'node_modules', '.bin');
       const checkScript = path.join(projectDir, 'check.js');

       fs.mkdirSync(nodeModulesBin, { recursive: true });
       fs.writeFileSync(packageJsonPath, JSON.stringify({
         scripts: {
           unicode: `"${process.execPath}" check.js`,
         },
       }));
       fs.writeFileSync(checkScript, `
         'use strict';
         console.log(JSON.stringify({
           cwd: process.cwd(),
           packageJsonPath: process.env.NODE_RUN_PACKAGE_JSON_PATH,
           path: process.env.PATH,
         }));
       `);

       const child = await common.spawnPromisified(
         process.execPath,
         [ '--run', 'unicode'],
         { cwd: projectDir },
       );

       assert.strictEqual(child.stderr, '');
       assert.strictEqual(child.code, 0);

       const output = JSON.parse(child.stdout);
       assert.strictEqual(output.cwd, projectDir);
       assert.strictEqual(output.packageJsonPath, packageJsonPath);
       assert.strictEqual(output.path.split(path.delimiter)[0], nodeModulesBin);
     });

  it('returns error on unparsable file', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'test'],
      { cwd: fixtures.path('run-script/cannot-parse') },
    );
    assert.match(child.stderr, /Can't parse/);
    assert.strictEqual(child.stdout, '');
    assert.strictEqual(child.code, 1);
  });

  it('returns error when there is no "scripts" field file', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'test'],
      { cwd: fixtures.path('run-script/cannot-find-script') },
    );
    assert.match(child.stderr, /Can't find "scripts" field in/);
    assert.strictEqual(child.stdout, '');
    assert.strictEqual(child.code, 1);
  });

  it('escapes shell characters', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', `positional-args${envSuffix}`, '--', '%PAYLOAD%', '$PAYLOAD'],
      { cwd: fixtures.path('run-script'), env: { ...process.env, PAYLOAD: 'env value' } },
    );
    assert.strictEqual(
      child.stdout,
      common.isWindows ?
        `Raw '"^%PAYLOAD^%" "$PAYLOAD"'\r\nArguments: '%PAYLOAD% $PAYLOAD'\r\nThe total number of arguments is: 2\r\n` :
        "Arguments: '%PAYLOAD% $PAYLOAD'\nThe total number of arguments is: 2\n");
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });
});
