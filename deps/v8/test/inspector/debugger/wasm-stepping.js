// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests stepping through wasm scripts');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let func_a_idx =
    builder.addFunction('wasm_A', kSig_v_v).addBody([kExprNop, kExprNop]).index;

// wasm_B calls wasm_A <param0> times.
builder.addFunction('wasm_B', kSig_v_i)
    .addBody([
      // clang-format off
      kExprLoop, kWasmStmt,               // while
        kExprLocalGet, 0,                 // -
        kExprIf, kWasmStmt,               // if <param0> != 0
          kExprLocalGet, 0,               // -
          kExprI32Const, 1,               // -
          kExprI32Sub,                    // -
          kExprLocalSet, 0,               // decrease <param0>
          kExprCallFunction, func_a_idx,  // -
          kExprBr, 1,                     // continue
          kExprEnd,                       // -
        kExprEnd,                         // break
      // clang-format on
    ])
    .exportAs('main');

let module_bytes = builder.toArray();

function instantiate(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }

  let module = new WebAssembly.Module(buffer);
  // Set global variable.
  instance = new WebAssembly.Instance(module);
}

let evalWithUrl = (code, url) => Protocol.Runtime.evaluate(
    {'expression': code + '\n//# sourceURL=v8://test/' + url});

Protocol.Debugger.onPaused(handlePaused);
let wasm_B_scriptId;
let step_actions = [
  'stepInto',  // == stepOver, to call instruction
  'stepInto',  // into call to wasm_A
  'stepOver',  // over first nop
  'stepOut',   // out of wasm_A
  'stepOut',   // out of wasm_B, stop on breakpoint again
  'stepOver',  // to call
  'stepOver',  // over call
  'resume',    // to next breakpoint (third iteration)
  'stepInto',  // to call
  'stepInto',  // into wasm_A
  'stepOut',   // out to wasm_B
  // now step 9 times, until we are in wasm_A again.
  'stepInto', 'stepInto', 'stepInto', 'stepInto', 'stepInto', 'stepInto',
  'stepInto', 'stepInto', 'stepInto',
  // 3 more times, back to wasm_B.
  'stepInto', 'stepInto', 'stepInto',
  // then just resume.
  'resume'
];
for (let action of step_actions) {
  InspectorTest.logProtocolCommandCalls('Debugger.' + action)
}
let sources = {};
let urls = {};
let afterTwoSourcesCallback;

(async function Test() {
  await Protocol.Debugger.enable();
  InspectorTest.log('Installing code an global variable.');
  await evalWithUrl('var instance;\n' + instantiate.toString(), 'setup');
  InspectorTest.log('Calling instantiate function.');
  evalWithUrl(
      'instantiate(' + JSON.stringify(module_bytes) + ')', 'callInstantiate');
  await waitForTwoWasmScripts();
  InspectorTest.log(
      'Setting breakpoint on line 7 (on the setlocal before the call), url ' +
      urls[wasm_B_scriptId]);
  let msg = await Protocol.Debugger.setBreakpoint(
      {'location': {'scriptId': wasm_B_scriptId, 'lineNumber': 7}});
  printFailure(msg);
  InspectorTest.logMessage(msg.result.actualLocation);
  await evalWithUrl('instance.exports.main(4)', 'runWasm');
  InspectorTest.log('exports.main returned!');
  InspectorTest.log('Finished!');
  InspectorTest.completeTest();
})();

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function waitForTwoWasmScripts() {
  let num = 0;
  InspectorTest.log('Waiting for two wasm scripts to be parsed.');
  let source_promises = [];
  async function getWasmSource(scriptId) {
    let msg = await Protocol.Debugger.getScriptSource({scriptId: scriptId});
    printFailure(msg);
    InspectorTest.log(msg.result.scriptSource);
    sources[scriptId] = msg.result.scriptSource;
  }
  while (num < 2) {
    let msg = await Protocol.Debugger.onceScriptParsed();
    let url = msg.params.url;
    if (!url.startsWith('wasm://') || url.split('/').length != 5) {
      InspectorTest.log('Ignoring script with url ' + url);
      continue;
    }
    num += 1;
    let scriptId = msg.params.scriptId;
    urls[scriptId] = url;
    InspectorTest.log('Got wasm script: ' + url);
    if (url.substr(-2) == '-1') wasm_B_scriptId = scriptId;
    InspectorTest.log('Requesting source for ' + url + '...');
    source_promises.push(getWasmSource(scriptId));
  }
  await Promise.all(source_promises);
}

function printPauseLocation(scriptId, lineNr, columnNr) {
  let lines = sources[scriptId].split('\n');
  let line = '<illegal line number>';
  if (lineNr < lines.length) {
    line = lines[lineNr];
    if (columnNr < line.length) {
      line = line.substr(0, columnNr) + '>' + line.substr(columnNr);
    }
  }
  InspectorTest.log(
      'Paused at ' + urls[scriptId] + ':' + lineNr + ':' + columnNr + ': ' +
      line);
}

async function getValueString(value) {
  if (value.type == 'object') {
    let msg = await Protocol.Runtime.callFunctionOn({
      objectId: value.objectId,
      functionDeclaration: 'function () { return JSON.stringify(this); }'
    });
    printFailure(msg);
    return msg.result.result.value + ' (' + value.description + ')';
  }
  return value.value + ' (' + value.type + ')';
}

async function dumpProperties(message) {
  printFailure(message);
  for (let value of message.result.result) {
    let value_str = await getValueString(value.value);
    InspectorTest.log('   ' + value.name + ': ' + value_str);
  }
}

async function dumpScopeChainsOnPause(message) {
  for (let frame of message.params.callFrames) {
    let functionName = frame.functionName || '(anonymous)';
    let lineNumber = frame.location ? frame.location.lineNumber : frame.lineNumber;
    let columnNumber = frame.location ? frame.location.columnNumber : frame.columnNumber;
    InspectorTest.log(`at ${functionName} (${lineNumber}:${columnNumber}):`);
    for (let scope of frame.scopeChain) {
      InspectorTest.logObject(' - scope (' + scope.type + '):');
      if (scope.type == 'global') {
        InspectorTest.logObject('   -- skipped');
      } else {
        let properties = await Protocol.Runtime.getProperties(
            {'objectId': scope.object.objectId});
        await dumpProperties(properties);
      }
    }
  }
}

async function handlePaused(msg) {
  let loc = msg.params.callFrames[0].location;
  printPauseLocation(loc.scriptId, loc.lineNumber, loc.columnNumber);
  await dumpScopeChainsOnPause(msg);
  let action = step_actions.shift() || 'resume';
  await Protocol.Debugger[action]();
}
