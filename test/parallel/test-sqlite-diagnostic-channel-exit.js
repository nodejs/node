'use strict';

// A statement left unfinished at exit is finalized by the destructor chain,
// which runs after the diagnostics channel binding has been destroyed. SQLite
// invokes the profile callback for such a statement, so the channel must not
// read through the destroyed binding.

const common = require('../common');
common.skipIfSQLiteMissing();

const { spawnSyncAndExitWithoutError } = require('../common/child_process');

// Stays reachable for the rest of the process so the statement is finalized
// during teardown rather than by the garbage collector.
const keepAlive = [];

function leaveStatementUnfinished() {
  const dc = require('node:diagnostics_channel');
  const { DatabaseSync } = require('node:sqlite');

  dc.subscribe('sqlite.db.query', () => {});

  const db = new DatabaseSync(':memory:');
  db.exec('CREATE TABLE t (x INTEGER)');
  const insert = db.prepare('INSERT INTO t VALUES (?)');
  for (let i = 0; i < 200; i++) insert.run(i);

  const iterator = db.prepare('SELECT x FROM t').iterate();
  iterator.next();
  keepAlive.push({ db, iterator });
}

switch (process.argv[2]) {
  case 'main':
    leaveStatementUnfinished();
    break;

  case 'worker': {
    const { Worker, isMainThread } = require('node:worker_threads');
    if (isMainThread) {
      new Worker(__filename, { argv: ['worker'] });
    } else {
      leaveStatementUnfinished();
    }
    break;
  }

  default:
    spawnSyncAndExitWithoutError(process.execPath, [__filename, 'main']);
    spawnSyncAndExitWithoutError(process.execPath, [__filename, 'worker']);
}
