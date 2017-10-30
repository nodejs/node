// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

let {session, contextGroup, Protocol} = InspectorTest.start('Tests breakable locations in wasm');

utils.load('test/mjsunit/wasm/wasm-constants.js');
utils.load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();

// clang-format off
var func_idx = builder.addFunction('helper', kSig_v_v)
    .addLocals({i32_count: 1})
    .addBody([
        kExprNop,
        kExprI32Const, 12,
        kExprSetLocal, 0,
    ]).index;

builder.addFunction('main', kSig_v_i)
    .addBody([
        kExprGetLocal, 0,
        kExprIf, kWasmStmt,
          kExprBlock, kWasmStmt,
            kExprCallFunction, func_idx,
          kExprEnd,
        kExprEnd
    ]).exportAs('main');
// clang-format on

var module_bytes = builder.toArray();

function testFunction(bytes) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; i++) {
    view[i] = bytes[i] | 0;
  }

  var module = new WebAssembly.Module(buffer);
  // Set global variable.
  instance = new WebAssembly.Instance(module);
}

var evalWithUrl = (code, url) => Protocol.Runtime.evaluate(
    {'expression': code + '\n//# sourceURL=v8://test/' + url});

var setupCode = testFunction.toString() + ';\nvar module_bytes = ' +
    JSON.stringify(module_bytes) + ';\nvar instance;';

Protocol.Debugger.enable();
Protocol.Debugger.onScriptParsed(handleScriptParsed);
InspectorTest.log('Running testFunction...');
evalWithUrl(setupCode, 'setup')
    .then(() => evalWithUrl('testFunction(module_bytes)', 'runTestFunction'))
    .then(getBreakableLocationsForAllWasmScripts)
    .then(setAllBreakableLocations)
    .then(() => InspectorTest.log('Running wasm code...'))
    .then(() => (evalWithUrl('instance.exports.main(1)', 'runWasm'), 0))
    .then(waitForAllPauses)
    .then(() => InspectorTest.log('Finished!'))
    .then(InspectorTest.completeTest);

var allBreakableLocations = [];

var urls = {};
var numScripts = 0;
var wasmScripts = [];
function handleScriptParsed(messageObject) {
  var scriptId = messageObject.params.scriptId;
  var url = messageObject.params.url;
  urls[scriptId] = url;
  InspectorTest.log('Script nr ' + numScripts + ' parsed. URL: ' + url);
  ++numScripts;

  if (url.startsWith('wasm://')) {
    InspectorTest.log('This is a wasm script (nr ' + wasmScripts.length + ').');
    wasmScripts.push(scriptId);
  }
}

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

function printBreakableLocations(message, expectedScriptId, source) {
  var lines = source.split('\n');
  var locations = message.result.locations;
  InspectorTest.log(locations.length + ' breakable location(s):');
  for (var i = 0; i < locations.length; ++i) {
    if (locations[i].scriptId != expectedScriptId) {
      InspectorTest.log(
          'SCRIPT ID MISMATCH!! ' + locations[i].scriptId + ' != ' +
          expectedScriptId);
    }
    var line = '<illegal line number>';
    if (locations[i].lineNumber < lines.length) {
      line = lines[locations[i].lineNumber];
      if (locations[i].columnNumber < line.length) {
        line = line.substr(0, locations[i].columnNumber) + '>' +
            line.substr(locations[i].columnNumber);
      }
    }
    InspectorTest.log(
        '[' + i + '] ' + locations[i].lineNumber + ':' +
        locations[i].columnNumber + ' || ' + line);
  }
}

function checkGetBreakableLocations(wasmScriptNr) {
  InspectorTest.log(
      'Requesting all breakable locations in wasm script ' + wasmScriptNr);
  var scriptId = wasmScripts[wasmScriptNr];
  var source;
  return Protocol.Debugger.getScriptSource({scriptId: scriptId})
      .then(msg => source = msg.result.scriptSource)
      .then(
          () => Protocol.Debugger.getPossibleBreakpoints(
              {start: {lineNumber: 0, columnNumber: 0, scriptId: scriptId}}))
      .then(printFailure)
      .then(msg => (allBreakableLocations.push(...msg.result.locations), msg))
      .then(msg => printBreakableLocations(msg, scriptId, source))
      .then(
          () => InspectorTest.log(
              'Requesting breakable locations in lines [0,3)'))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({
        start: {lineNumber: 0, columnNumber: 0, scriptId: scriptId},
        end: {lineNumber: 3, columnNumber: 0, scriptId: scriptId}
      }))
      .then(printFailure)
      .then(msg => printBreakableLocations(msg, scriptId, source))
      .then(
          () => InspectorTest.log(
              'Requesting breakable locations in lines [4,6)'))
      .then(() => Protocol.Debugger.getPossibleBreakpoints({
        start: {lineNumber: 4, columnNumber: 0, scriptId: scriptId},
        end: {lineNumber: 6, columnNumber: 0, scriptId: scriptId}
      }))
      .then(printFailure)
      .then(msg => printBreakableLocations(msg, scriptId, source));
}

function getBreakableLocationsForAllWasmScripts() {
  InspectorTest.log('Querying breakable locations for all wasm scripts now...');
  var promise = Promise.resolve();
  for (var wasmScriptNr = 0; wasmScriptNr < wasmScripts.length;
       ++wasmScriptNr) {
    promise = promise.then(checkGetBreakableLocations.bind(null, wasmScriptNr));
  }
  return promise;
}

function locationMatches(loc1, loc2) {
  return loc1.scriptId == loc2.scriptId && loc1.lineNumber == loc2.lineNumber &&
      loc1.columnNumber == loc2.columnNumber;
}

function locationStr(loc) {
  return urls[loc.scriptId] + ':' + loc.lineNumber + ':' + loc.columnNumber;
}

function setBreakpoint(loc) {
  InspectorTest.log('Setting at ' + locationStr(loc));
  function check(msg) {
    if (locationMatches(loc, msg.result.actualLocation)) {
      InspectorTest.log("Success!");
    } else {
      InspectorTest.log("Mismatch!");
      InspectorTest.logMessage(msg);
    }
  }
  return Protocol.Debugger.setBreakpoint({'location': loc})
      .then(printFailure)
      .then(check);
}

function setAllBreakableLocations() {
  InspectorTest.log('Setting a breakpoint on each breakable location...');
  var promise = Promise.resolve();
  for (var loc of allBreakableLocations) {
    promise = promise.then(setBreakpoint.bind(null, loc));
  }
  return promise;
}

function removePausedLocation(msg) {
  var topLocation = msg.params.callFrames[0].location;
  InspectorTest.log('Stopped at ' + locationStr(topLocation));
  for (var i = 0; i < allBreakableLocations.length; ++i) {
    if (locationMatches(topLocation, allBreakableLocations[i])) {
      allBreakableLocations.splice(i, 1);
      --i;
    }
  }
}

function waitForAllPauses() {
  InspectorTest.log('Missing breakpoints: ' + allBreakableLocations.length);
  if (allBreakableLocations.length == 0) return;
  return Protocol.Debugger.oncePaused()
      .then(removePausedLocation)
      .then(Protocol.Debugger.resume())
      .then(waitForAllPauses);
}
