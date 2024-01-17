'use strict';
const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const als = new AsyncLocalStorage();

function getStore() {
  return als.getStore();
}

common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');
const path = require('path');
const { pathToFileURL } = require('url');

let valueInFunction = 0;
let valueInBreakpoint = 0;

function debugged() {
  valueInFunction = getStore();
  console.log('in code => ', getStore());
  return 42;
}

async function test() {
  const session = new Session();

  session.connect();
  console.log('Connected');

  session.post('Debugger.enable');
  console.log('Debugger was enabled');

  session.on('Debugger.paused', () => {
    valueInBreakpoint = getStore();
    console.log('on Debugger.paused callback => ', getStore());
  });

  await new Promise((resolve, reject) => {
    session.post('Debugger.setBreakpointByUrl', {
      'lineNumber': 22,
      'url': pathToFileURL(path.resolve(__dirname, __filename)).toString(),
      'columnNumber': 0,
      'condition': ''
    }, (error, result) => {
      return error ? reject(error) : resolve(result);
    });
  });
  console.log('Breakpoint was set');

  als.run(1, debugged);
  assert.strictEqual(valueInFunction, valueInBreakpoint);
  assert.strictEqual(valueInFunction, 1);
  assert.notStrictEqual(valueInFunction, 0);
  assert.notStrictEqual(valueInBreakpoint, 0);

  console.log('Breakpoint was hit');

  session.disconnect();
  console.log('Session disconnected');
}

const interval = setInterval(() => {}, 1000);
test().then(common.mustCall(() => {
  clearInterval(interval);
  console.log('Done!');
}));
