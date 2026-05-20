'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const { startNewREPLServer } = require('../common/repl');

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

const { replServer } = startNewREPLServer();

// Tab completion for files/directories
{
  process.chdir(__dirname);

  const readFileSyncs = ['fs.readFileSync("', 'fs.promises.readFileSync("'];
  if (!common.isWindows) {
    readFileSyncs.forEach((readFileSync) => {
      const fixturePath = `${readFileSync}../fixtures/test-repl-tab-completion`;
      replServer.complete(
        fixturePath,
        common.mustCall((err, data) => {
          assert.strictEqual(err, null);
          assert.ok(data[0][0].includes('.hiddenfiles'));
          assert.ok(data[0][1].includes('hellorandom.txt'));
          assert.ok(data[0][2].includes('helloworld.js'));
        })
      );

      replServer.complete(
        `${fixturePath}/hello`,
        common.mustCall((err, data) => {
          assert.strictEqual(err, null);
          assert.ok(data[0][0].includes('hellorandom.txt'));
          assert.ok(data[0][1].includes('helloworld.js'));
        })
      );

      replServer.complete(
        `${fixturePath}/.h`,
        common.mustCall((err, data) => {
          assert.strictEqual(err, null);
          assert.ok(data[0][0].includes('.hiddenfiles'));
        })
      );

      replServer.complete(
        `${readFileSync}./xxxRandom/random`,
        common.mustCall((err, data) => {
          assert.strictEqual(err, null);
          assert.strictEqual(data[0].length, 0);
        })
      );

      const testPath = fixturePath.slice(0, -1);
      replServer.complete(
        testPath,
        common.mustCall((err, data) => {
          assert.strictEqual(err, null);
          assert.ok(data[0][0].includes('test-repl-tab-completion'));
          assert.strictEqual(data[1], path.basename(testPath));
        })
      );
    });
  }
}
