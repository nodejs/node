// This worker is used for one of the tests in test-sqlite-session.js

'use strict';
require('../common');
const { parentPort, workerData } = require('worker_threads');
const { DatabaseSync, constants } = require('node:sqlite');
const { changeset, mode, dbPath } = workerData;

const db = new DatabaseSync(dbPath);

const options = {};
if (mode !== constants.SQLITE_CHANGESET_ABORT && mode !== constants.SQLITE_CHANGESET_OMIT) {
  throw new Error('Unexpected value for mode');
}
options.onConflict = () => mode;

try {
  const result = db.applyChangeset(changeset, options);
  parentPort.postMessage({ mode, result, error: null });
} catch (error) {
  parentPort.postMessage({ mode, result: null, errorMessage: error.message, errcode: error.errcode });
} finally {
  db.close();  // Just to make sure it is closed ASAP
}
