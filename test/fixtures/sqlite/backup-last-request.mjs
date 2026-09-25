import { backup, DatabaseSync } from 'node:sqlite';

const source = new DatabaseSync(':memory:');
source.exec(`
  CREATE TABLE data(value);
  INSERT INTO data VALUES (zeroblob(1048576));
`);

let keepAlive = setInterval(() => {}, 1_000);
let settled = false;

process.once('beforeExit', () => {
  if (!settled) {
    process.stderr.write('backup promise did not settle before the event loop became idle\n');
    process.exit(1);
  }
});

backup(source, process.argv[2], {
  rate: 1,
  progress() {
    if (keepAlive !== undefined) {
      clearInterval(keepAlive);
      keepAlive = undefined;
    }
  },
}).then(() => {
  settled = true;
  source.close();
}, (error) => {
  settled = true;
  process.stderr.write(`${error.stack ?? error}\n`);
  process.exitCode = 1;
});
