'use strict';
const common = require('../common.js');
const tmpdir = require('../../test/common/tmpdir');
const path = require('path');
const fsp = require('fs/promises');
const fs = require('fs');


const KB = 2 ** 10;
const MB = 2 ** 20;

const configs = {
  n: [100],
  fileSize: [
    1 * KB,
    1 * MB,
    10 * MB,
  ],
};

tmpdir.refresh();
const sourceRoot = path.resolve(tmpdir.path, 'source');
const destRoot = path.resolve(tmpdir.path, 'dest');

const bench = common.createBenchmark(main, configs);

async function main(conf) {
  const { fileSize } = conf;
  const content = 'a'.repeat(fileSize);
  const sourceName = `${sourceRoot}-${fileSize}`;
  const destName = `${destRoot}-${fileSize}`;

  const testFixtures = await Promise.all(
    new Array(conf.n).fill(null).map((_, i) => prepareFixture(i))
  );

  bench.start();
  await Promise.all(
    testFixtures.map(
      (testFixture) => testFixture[0].stream(testFixture[1])
    )
  );
  bench.end(conf.n);

  await Promise.all(testFixtures.map((_, i) => {
    return cleanUp(i);
  }));

  async function prepareFixture(i) {
    await fsp.writeFile(`${sourceName}-${i}`, content);
    const destStream = fs.createWriteStream(`${destName}-${i}`);
    const sourceHandle = await fsp.open(`${sourceName}-${i}`);
    return [sourceHandle, destStream];
  }

  async function cleanUp(i) {
    await Promise.all([fsp.unlink(`${sourceName}-${i}`), fsp.unlink(`${destName}-${i}`)]);
  }
}
