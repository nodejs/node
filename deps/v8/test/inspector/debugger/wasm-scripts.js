// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

InspectorTest.log("Tests how wasm scripts are reported");

let contextGroup = new InspectorTest.ContextGroup();
let sessions = [
  // Main session.
  trackScripts(),
  // Extra session to verify that all inspectors get same messages.
  // See https://bugs.chromium.org/p/v8/issues/detail?id=9725.
  trackScripts(),
];

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

// Create module with given custom sections.
function createModule(...customSections) {
  var builder = new WasmModuleBuilder();
  builder.addFunction('nopFunction', kSig_v_v).addBody([kExprNop]);
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprBlock, kWasmStmt, kExprI32Const, 2, kExprDrop, kExprEnd])
      .exportAs('main');
  for (var { name, value } of customSections) {
    builder.addCustomSection(name, value);
  }
  return builder.toArray();
}

function testFunction(bytes) {
  // Compilation triggers registration of wasm scripts.
  new WebAssembly.Module(new Uint8Array(bytes));
}

contextGroup.addScript(testFunction.toString(), 0, 0, 'v8://test/testFunction');

InspectorTest.log(
    'Check that each inspector gets a wasm script at module creation time.');

// Sample .debug_info section.
// Content doesn't matter, as we don't try to parse it in V8,
// but should be non-empty to check that we're skipping it correctly.
const dwarfSection = { name: '.debug_info', value: [1, 2, 3, 4, 5] };

// Sample sourceMappingURL section set to "abc".
const sourceMapSection = { name: 'sourceMappingURL', value: [3, 97, 98, 99] };

sessions[0].Protocol.Runtime
    .evaluate({
      'expression': `//# sourceURL=v8://test/runTestRunction

      // no debug info
      testFunction([${createModule()}]);

      // DWARF
      testFunction([${createModule(dwarfSection)}]);

      // Source map
      testFunction([${createModule(sourceMapSection)}]);

      // DWARF + source map
      testFunction([${createModule(dwarfSection, sourceMapSection)}]);

      // Source map + DWARF (different order)
      testFunction([${createModule(sourceMapSection, dwarfSection)}]);
      `
    })
    .then(() => (
      // At this point all scripts were parsed.
      // Stop tracking and wait for script sources in each session.
      Promise.all(sessions.map(session => session.getScripts()))
    ))
    .catch(err => {
      InspectorTest.log(err.stack);
    })
    .then(() => InspectorTest.completeTest());

function trackScripts(debuggerParams) {
  let {id: sessionId, Protocol} = contextGroup.connect();
  let scripts = [];

  Protocol.Debugger.enable(debuggerParams);
  Protocol.Debugger.onScriptParsed(handleScriptParsed);

  async function loadScript({url, scriptId, sourceMapURL}) {
    InspectorTest.log(`Session #${sessionId}: Script #${scripts.length} parsed. URL: ${url}. Source map URL: ${sourceMapURL}`);
    let {result: {scriptSource, bytecode}} = await Protocol.Debugger.getScriptSource({scriptId});
    if (bytecode) {
      if (scriptSource) {
        InspectorTest.log('Unexpected scriptSource with bytecode: ');
        InspectorTest.log(scriptSource);
      }
      // Binary value is represented as base64 in JSON, decode it.
      bytecode = InspectorTest.decodeBase64(bytecode);
      // Check that it can be parsed back to a WebAssembly module.
      let module = new WebAssembly.Module(bytecode);
      scriptSource = `
Raw: ${Array.from(bytecode, b => ('0' + b.toString(16)).slice(-2)).join(' ')}
Imports: [${WebAssembly.Module.imports(module).map(i => `${i.name}: ${i.kind} from "${i.module}"`).join(', ')}]
Exports: [${WebAssembly.Module.exports(module).map(e => `${e.name}: ${e.kind}`).join(', ')}]
      `.trim();
    }
    InspectorTest.log(`Session #${sessionId}: Source for ${url}:`);
    InspectorTest.log(scriptSource);
  }

  function handleScriptParsed({params}) {
    if (params.url.startsWith('wasm://')) {
      scripts.push(loadScript(params));
    }
  }

  return {
    Protocol,
    getScripts: () => Promise.all(scripts),
  };
}
