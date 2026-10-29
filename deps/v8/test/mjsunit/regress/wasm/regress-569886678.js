// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --wasm-custom-descriptors --wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/wasm/prototype-setup-builder.js');

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

let exception_paused = false;

function receive(message) {
  let msg = JSON.parse(message);
  if (msg.error) {
    throw new Error('Inspector error: ' + JSON.stringify(msg.error));
  }
  if (msg.id === 1) {
    send(JSON.stringify({
      id: 2,
      method: 'Debugger.setPauseOnExceptions',
      params: {state: 'caught'}
    }));
  } else if (msg.id === 2) {
    runTrigger();
  } else if (msg.method === 'Debugger.paused') {
    exception_paused = true;
    send(JSON.stringify({id: 3, method: 'Debugger.resume'}));
  }
}

function runTrigger() {
  try {
    proto.myMethod.call(Symbol("sym"));
  } catch(e) {
  }
}

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));

assertTrue(exception_paused);
