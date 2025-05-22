'use strict';

const common = require('../common');
common.requireNoPackageJSONAbove();

const { it, describe } = require('node:test');
const assert = require('node:assert');

const fixtures = require('../common/fixtures');
const envSuffix = common.isWindows ? '-windows' : '';

const path = require('node:path');
const nodeDir = path.dirname(process.execPath);
const env = { ...process.env, PATH: `${nodeDir}${path.delimiter}${process.env.PATH}` };

describe('node --run [command]', () => {
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
    // Scripts take precedence over bins
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'test', '--no-warnings'],
      { cwd: fixtures.path('run-script') },
    );
    assert.match(child.stdout, /Error: no test specified/);
    assert.strictEqual(child.code, 1);
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
      [ '--run', `positional-args${envSuffix}`, '--', '--help "hello world test"', 'A', 'B', 'C'],
      { cwd: fixtures.path('run-script') },
    );
    if (common.isWindows) {
      assert.match(child.stdout, /Arguments: '--help ""hello world test"" A B C'/);
    } else {
      assert.match(child.stdout, /Arguments: '--help "hello world test" A B C'/);
    }
    assert.match(child.stdout, /The total number of arguments are: 4/);
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

  it('returns error when there is no "scripts" and "bin" fields in file', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'test'],
      { cwd: fixtures.path('run-script/cannot-find-script-and-bin') },
    );
    assert.match(child.stderr, /Can't find "scripts" or "bin" fields in/);
    assert.strictEqual(child.stdout, '');
    assert.strictEqual(child.code, 1);
  });

  it('print avilables scripts and bins when command not found', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--run', 'tmp'],
      { cwd: fixtures.path('run-script') },
    );
    assert.match(child.stderr, /Unknown script or bin entry "tmp"/);
    assert.match(child.stderr, /Available scripts:\n/);
    assert.match(child.stderr, /ada: ada\n/);
    assert.match(child.stderr, /Available bins:\n/);
    assert.match(child.stderr, /bin-test: \.\/test\.js\n/);
  });

  describe('Bin scripts use cases', () => {
    it('runs a bin from package.json object format', async () => {
      const child = await common.spawnPromisified(
        process.execPath,
        ['--run', 'bin-test'],
        { cwd: fixtures.path('run-script'), env },
      );
      assert.match(child.stdout, /bin-test script/);
    });

    it('handles error with invalid bin value in object format', async () => {
      const child = await common.spawnPromisified(
        process.execPath,
        ['--run', 'invalid-bin'],
        { cwd: fixtures.path('run-script/invalid-bin-value'), env },
      );
      assert.match(child.stderr, /Bin "invalid-bin" is unexpectedly not a string/);
    });

    it('runs a bin from package.json string format', async () => {
      const child = await common.spawnPromisified(
        process.execPath,
        ['--run', 'bin-test'],
        { cwd: fixtures.path('run-script/bin-string'), env },
      );
      assert.match(child.stdout, /bin-test script/);
    });

    it('handles error with invalid bin value in string format', async () => {
      const child = await common.spawnPromisified(
        process.execPath,
        ['--run', 'invalid-bin'],
        { cwd: fixtures.path('run-script/invalid-schema'), env }
      );
      assert.match(child.stderr, /Bin "invalid-bin" is unexpectedly not a string/);
    });

    it('adds node_modules/.bin to path', async () => {
      const child = await common.spawnPromisified(
        process.execPath,
        ['--run', `bin-ada${envSuffix}`],
        { cwd: fixtures.path('run-script') },
      );
      assert.match(child.stdout, /06062023/);
      assert.strictEqual(child.stderr, '');
      assert.strictEqual(child.code, 0);
    });
  });
});
