const { DatabaseSync } = require('node:sqlite');

const dbPath = process.argv[2];
const conn = new DatabaseSync(dbPath);

conn.exec('BEGIN EXCLUSIVE TRANSACTION');
process.send('locked');
setTimeout(() => {
  conn.exec('ROLLBACK');
  conn.close();
  process.exit(0);
}, 1000);