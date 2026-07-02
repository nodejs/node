'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');
const path = require('path');
const { pathToFileURL } = require('url');

function debugged() {
  return 42;
}

async function test() {
  // Check without properly decoding
  const session1 = new Session();
  session1.connect();
  let session1Paused = false;
  session1.on('Debugger.paused', () => session1Paused = true);
  session1.post('Debugger.enable');

  await new Promise((resolve, reject) => {
    session1.post('Debugger.setBreakpointByUrl', {
      'lineNumber': 12,
      'url': pathToFileURL(path.resolve(__dirname, __filename)).toString(),
      'columnNumber': 0,
      'condition': ''
    }, (error, result) => {
      return error ? reject(error) : resolve(result);
    });
  });

  debugged();

  assert(!session1Paused);
  session1.disconnect();

  // Check correctly with another session
  let session2Paused = false;
  const session2 = new Session();
  session2.connect();
  session2.on('Debugger.paused', () => session2Paused = true);
  session2.post('Debugger.enable');

  await new Promise((resolve, reject) => {
    session2.post('Debugger.setBreakpointByUrl', {
      'lineNumber': 12,
      'url': decodeURIComponent(pathToFileURL(path.resolve(__dirname, __filename)).toString()),
      'columnNumber': 0,
      'condition': ''
    }, (error, result) => {
      return error ? reject(error) : resolve(result);
    });
  });

  debugged();

  assert(session2Paused);
  session2.disconnect();
}

const interval = setInterval(() => {}, 1000);
test().then(common.mustCall(() => {
  clearInterval(interval);
  console.log('Done!');
}));
