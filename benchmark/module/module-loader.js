'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

const tmpDirectory = path.join(__dirname, '..', 'tmp');
const benchmarkDirectory = path.join(tmpDirectory, 'nodejs-benchmark-module');

const bench = common.createBenchmark(main, {
  thousands: [50],
  fullPath: ['true', 'false'],
  useCache: ['true', 'false']
});

function main(conf) {
  const n = +conf.thousands * 1e3;

  rmrf(tmpDirectory);
  try { fs.mkdirSync(tmpDirectory); } catch (e) {}
  try { fs.mkdirSync(benchmarkDirectory); } catch (e) {}

  for (var i = 0; i <= n; i++) {
    fs.mkdirSync(`${benchmarkDirectory}${i}`);
    fs.writeFileSync(
      `${benchmarkDirectory}${i}/package.json`,
      '{"main": "index.js"}'
    );
    fs.writeFileSync(
      `${benchmarkDirectory}${i}/index.js`,
      'module.exports = "";'
    );
  }

  if (conf.fullPath === 'true')
    measureFull(n, conf.useCache === 'true');
  else
    measureDir(n, conf.useCache === 'true');

  rmrf(tmpDirectory);
}

function measureFull(n, useCache) {
  var i;
  if (useCache) {
    for (i = 0; i <= n; i++) {
      require(`${benchmarkDirectory}${i}/index.js`);
    }
  }
  bench.start();
  for (i = 0; i <= n; i++) {
    require(`${benchmarkDirectory}${i}/index.js`);
  }
  bench.end(n / 1e3);
}

function measureDir(n, useCache) {
  var i;
  if (useCache) {
    for (i = 0; i <= n; i++) {
      require(`${benchmarkDirectory}${i}`);
    }
  }
  bench.start();
  for (i = 0; i <= n; i++) {
    require(`${benchmarkDirectory}${i}`);
  }
  bench.end(n / 1e3);
}

function rmrf(location) {
  try {
    const things = fs.readdirSync(location);
    things.forEach(function(thing) {
      var cur = path.join(location, thing),
        isDirectory = fs.statSync(cur).isDirectory();
      if (isDirectory) {
        rmrf(cur);
        return;
      }
      fs.unlinkSync(cur);
    });
    fs.rmdirSync(location);
  } catch (err) {
    // Ignore error
  }
}
