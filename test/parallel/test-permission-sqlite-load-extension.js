'use strict';
const common = require('../common');
const assert = require('node:assert');
const childProcess = require('child_process');

const code = `const sqlite = require('node:sqlite');
const db = new sqlite.DatabaseSync(':memory:', { allowExtension: true });
db.loadExtension('nonexistent');`.replace(/\n/g, ' ');

childProcess.exec(
  `${process.execPath} --permission -e "${code}"`,
  {},
  common.mustCall((err, _, stderr) => {
    assert.strictEqual(err.code, 1);
    assert.match(stderr, /Error: Cannot load SQLite extensions when the permission model is enabled/);
    assert.match(stderr, /code: 'ERR_LOAD_SQLITE_EXTENSION'/);
  })
);
