'use strict';
const path = require('path');
const { mkdirSync, writeFileSync } = require('fs');
const hasInspector = process.config.variables.v8_enable_inspector === 1;
let inspector = null;
if (hasInspector) inspector = require('inspector');

let session;

function writeCoverage() {
  if (!session) {
    return;
  }

  const { threadId } = require('internal/worker');

  const filename = `coverage-${process.pid}-${Date.now()}-${threadId}.json`;
  try {
    // TODO(bcoe): switch to mkdirp once #22302 is addressed.
    mkdirSync(process.env.NODE_V8_COVERAGE);
  } catch (err) {
    if (err.code !== 'EEXIST') {
      console.error(err);
      return;
    }
  }

  const target = path.join(process.env.NODE_V8_COVERAGE, filename);

  try {
    session.post('Profiler.takePreciseCoverage', (err, coverageInfo) => {
      if (err) return console.error(err);
      try {
        writeFileSync(target, JSON.stringify(coverageInfo));
      } catch (err) {
        console.error(err);
      }
    });
  } catch (err) {
    console.error(err);
  } finally {
    session.disconnect();
    session = null;
  }
}

exports.writeCoverage = writeCoverage;

function setup() {
  if (!hasInspector) {
    console.warn('coverage currently only supported in main thread');
    return;
  }

  session = new inspector.Session();
  session.connect();
  session.post('Profiler.enable');
  session.post('Profiler.startPreciseCoverage', { callCount: true,
                                                  detailed: true });

  const reallyReallyExit = process.reallyExit;

  process.reallyExit = function(code) {
    writeCoverage();
    reallyReallyExit(code);
  };

  process.on('exit', writeCoverage);
}

exports.setup = setup;
