'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const helper = require('./inspector-helper.js');

let scopeId;

function checkListResponse(err, response) {
  assert.ifError(err);
  assert.strictEqual(1, response.length);
  assert.ok(response[0]['devtoolsFrontendUrl']);
  assert.ok(
    response[0]['webSocketDebuggerUrl']
      .match(/ws:\/\/127.0.0.1:\d+\/[0-9A-Fa-f]{8}-/));
}

function checkVersion(err, response) {
  assert.ifError(err);
  assert.ok(response);
  const expected = {
    'Browser': 'node.js/' + process.version,
    'Protocol-Version': '1.1',
  };
  assert.strictEqual(JSON.stringify(response),
                     JSON.stringify(expected));
}

function checkBadPath(err, response) {
  assert(err instanceof SyntaxError);
  assert(/Unexpected token/.test(err.message));
  assert(/WebSockets request was expected/.test(err.response));
}

function expectMainScriptSource(result) {
  const expected = helper.mainScriptSource();
  const source = result['scriptSource'];
  assert(source && (source.indexOf(expected) >= 0),
         'Script source is wrong: ' + source);
}

function setupExpectBreakOnLine(line, url, session, scopeIdCallback) {
  return function(message) {
    if ('Debugger.paused' === message['method']) {
      const callFrame = message['params']['callFrames'][0];
      const location = callFrame['location'];
      assert.strictEqual(url, session.scriptUrlForId(location['scriptId']));
      assert.strictEqual(line, location['lineNumber']);
      scopeIdCallback &&
          scopeIdCallback(callFrame['scopeChain'][0]['object']['objectId']);
      return true;
    }
  };
}

function setupExpectConsoleOutput(type, values) {
  if (!(values instanceof Array))
    values = [ values ];
  return function(message) {
    if ('Runtime.consoleAPICalled' === message['method']) {
      const params = message['params'];
      if (params['type'] === type) {
        let i = 0;
        for (const value of params['args']) {
          if (value['value'] !== values[i++])
            return false;
        }
        return i === values.length;
      }
    }
  };
}

function setupExpectScopeValues(expected) {
  return function(result) {
    for (const actual of result['result']) {
      const value = expected[actual['name']];
      if (value)
        assert.strictEqual(value, actual['value']['value']);
    }
  };
}

function setupExpectValue(value) {
  return function(result) {
    assert.strictEqual(value, result['result']['value']);
  };
}

function testBreakpointOnStart(session) {
  console.log('[test]',
              'Verifying debugger stops on start (--debug-brk option)');
  const commands = [
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Debugger.setPauseOnExceptions',
      'params': {'state': 'none'} },
    { 'method': 'Debugger.setAsyncCallStackDepth',
      'params': {'maxDepth': 0} },
    { 'method': 'Profiler.enable' },
    { 'method': 'Profiler.setSamplingInterval',
      'params': {'interval': 100} },
    { 'method': 'Debugger.setBlackboxPatterns',
      'params': {'patterns': []} },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ];

  session
    .sendInspectorCommands(commands)
    .expectMessages(setupExpectBreakOnLine(0, session.mainScriptPath, session));
}

function testSetBreakpointAndResume(session) {
  console.log('[test]', 'Setting a breakpoint and verifying it is hit');
  const commands = [
      { 'method': 'Debugger.setBreakpointByUrl',
        'params': { 'lineNumber': 5,
                    'url': session.mainScriptPath,
                    'columnNumber': 0,
                    'condition': ''
        }
      },
      { 'method': 'Debugger.resume'},
      [ { 'method': 'Debugger.getScriptSource',
          'params': { 'scriptId': session.mainScriptId } },
        expectMainScriptSource ],
  ];
  session
    .sendInspectorCommands(commands)
    .expectMessages([
      setupExpectConsoleOutput('log', ['A message', 5]),
      setupExpectBreakOnLine(5, session.mainScriptPath,
                             session, (id) => scopeId = id),
    ]);
}

function testInspectScope(session) {
  console.log('[test]', 'Verify we can read current application state');
  session.sendInspectorCommands([
    [
      {
        'method': 'Runtime.getProperties',
        'params': {
          'objectId': scopeId,
          'ownProperties': false,
          'accessorPropertiesOnly': false,
          'generatePreview': true
        }
      }, setupExpectScopeValues({t: 1001, k: 1})
    ],
    [
      {
        'method': 'Debugger.evaluateOnCallFrame', 'params': {
          'callFrameId': '{"ordinal":0,"injectedScriptId":1}',
          'expression': 'k + t',
          'objectGroup': 'console',
          'includeCommandLineAPI': true,
          'silent': false,
          'returnByValue': false,
          'generatePreview': true
        }
      }, setupExpectValue(1002)
    ],
    [
      {
        'method': 'Runtime.evaluate', 'params': {
          'expression': '5 * 5'
        }
      }, (message) => assert.strictEqual(25, message['result']['value'])
    ],
  ]);
}

function testNoUrlsWhenConnected(session) {
  session.testHttpResponse('/json/list', (err, response) => {
    assert.ifError(err);
    assert.strictEqual(1, response.length);
    assert.ok(!response[0].hasOwnProperty('devtoolsFrontendUrl'));
    assert.ok(!response[0].hasOwnProperty('webSocketDebuggerUrl'));
  });
}

function testI18NCharacters(session) {
  console.log('[test]', 'Verify sending and receiving UTF8 characters');
  const chars = 'טֶ字и';
  session.sendInspectorCommands([
    {
      'method': 'Debugger.evaluateOnCallFrame', 'params': {
        'callFrameId': '{"ordinal":0,"injectedScriptId":1}',
        'expression': 'console.log("' + chars + '")',
        'objectGroup': 'console',
        'includeCommandLineAPI': true,
        'silent': false,
        'returnByValue': false,
        'generatePreview': true
      }
    }
  ]).expectMessages([
    setupExpectConsoleOutput('log', [chars]),
  ]);
}

function testWaitsForFrontendDisconnect(session, harness) {
  console.log('[test]', 'Verify node waits for the frontend to disconnect');
  session.sendInspectorCommands({ 'method': 'Debugger.resume'})
    .expectStderrOutput('Waiting for the debugger to disconnect...')
    .disconnect(true);
}

function runTests(harness) {
  harness
    .testHttpResponse(null, '/json', checkListResponse)
    .testHttpResponse(null, '/json/list', checkListResponse)
    .testHttpResponse(null, '/json/version', checkVersion)
    .testHttpResponse(null, '/json/activate', checkBadPath)
    .testHttpResponse(null, '/json/activate/boom', checkBadPath)
    .testHttpResponse(null, '/json/badpath', checkBadPath)
    .runFrontendSession([
      testNoUrlsWhenConnected,
      testBreakpointOnStart,
      testSetBreakpointAndResume,
      testInspectScope,
      testI18NCharacters,
      testWaitsForFrontendDisconnect
    ]).expectShutDown(55);
}

helper.startNodeForInspectorTest(runTests);
