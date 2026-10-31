// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-js-interop

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/569886678.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));
contextGroup.addScript(
    utils.read('test/mjsunit/wasm/prototype-setup-builder.js'));

contextGroup.addScript(`
let builder = new WasmModuleBuilder();
let proto_config = new WasmPrototypeSetupBuilder(builder);

// We need a JS-to-wasm method wrapper to trigger the defect.
// The defect requires the parameter conversion to fail, so we create a
// method taking an i32 and pass it a Symbol receiver.
let $g_proto = builder.addImportedGlobal('p', 'proto', kWasmExternRef);
let $myMethod = builder.addFunction("myMethod", kSig_v_i).addBody([]);
proto_config.addConfig($g_proto)
            .addMethod('myMethod', kWasmMethod, $myMethod);
proto_config.build();

const constructors = {};
const proto = {};
builder.instantiate({ c: { constructors }, p: { proto } }, { builtins: ['js-prototypes'] });

function runTrigger() {
  try {
    proto.myMethod.call(Symbol("sym"));
  } catch(e) {
  }
}
`);

Protocol.Debugger.onPaused(message => {
  InspectorTest.log(
      `Paused on ${message.params.reason}: ${message.params.data.description.split('\n')[0]}`);
  Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'caught'});
  await Protocol.Runtime.evaluate({expression: 'runTrigger()'});
  InspectorTest.completeTest();
})();
