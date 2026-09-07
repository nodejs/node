'use strict';

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const allowed = tmpdir.resolve('allowed');
const blocked = tmpdir.resolve('blocked');
fs.mkdirSync(path.join(allowed, 'a', 'b'), { recursive: true });
fs.mkdirSync(path.join(allowed, 'c'));
fs.mkdirSync(blocked);
fs.writeFileSync(path.join(allowed, 'top'), '');
fs.writeFileSync(path.join(allowed, 'a', '1'), '');
fs.writeFileSync(path.join(allowed, 'a', 'b', '2'), '');
fs.writeFileSync(path.join(blocked, 'secret'), '');
if (common.canCreateSymLink()) {
  fs.symlinkSync(path.join(allowed, 'a'), path.join(allowed, 'c', 'link-to-a'), 'dir');
}

// Computed without the permission model, so by the native walk.
const expected = fs.readdirSync(allowed, { recursive: true });
assert(expected.includes(path.join('a', 'b', '2')));

const { status, stderr } = spawnSync(process.execPath, [
  '--permission',
  `--allow-fs-read=${allowed}`,
  '-e',
  `
    const assert = require('assert');
    const fs = require('fs');
    const path = require('path');
    const { ALLOWED, BLOCKED } = process.env;
    const expected = JSON.parse(process.env.EXPECTED);
    const denied = {
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemRead',
      resource: path.toNamespacedPath(BLOCKED),
    };

    function check(result, withFileTypes) {
      if (!withFileTypes) {
        assert.deepStrictEqual(result, expected);
        return;
      }
      assert(result.every((dirent) => dirent instanceof fs.Dirent));
      assert.deepStrictEqual(
        result.map((dirent) => path.join(path.relative(ALLOWED, dirent.parentPath), dirent.name)),
        expected,
      );
    }

    let pending = 0;
    process.on('exit', () => assert.strictEqual(pending, 0));
    for (const withFileTypes of [false, true]) {
      const options = { recursive: true, withFileTypes };
      check(fs.readdirSync(ALLOWED, options), withFileTypes);
      assert.throws(() => fs.readdirSync(BLOCKED, options), denied);

      pending += 4;
      fs.readdir(ALLOWED, options, (err, result) => {
        assert.ifError(err);
        check(result, withFileTypes);
        pending--;
      });
      fs.readdir(BLOCKED, options, (err) => {
        assert.throws(() => { throw err; }, denied);
        pending--;
      });
      fs.promises.readdir(ALLOWED, options).then((result) => {
        check(result, withFileTypes);
        pending--;
      });
      assert.rejects(fs.promises.readdir(BLOCKED, options), denied).then(() => pending--);
    }
  `,
], {
  env: {
    ...process.env,
    ALLOWED: allowed,
    BLOCKED: blocked,
    EXPECTED: JSON.stringify(expected),
  },
});
assert.strictEqual(status, 0, stderr.toString());
