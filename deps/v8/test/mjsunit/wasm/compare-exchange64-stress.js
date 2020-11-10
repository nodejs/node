// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-threads

load("test/mjsunit/wasm/wasm-module-builder.js");

const kSequenceLength = 8192;
const kNumberOfWorkers = 4;
const kBitMask = kNumberOfWorkers - 1;
const kSequenceStartAddress = 32;

function makeWorkerCodeForOpcode(compareExchangeOpcode, size, functionName,
    builder) {
    let loadMemOpcode = kTrapUnreachable;
    switch (size) {
        case 64:
            loadMemOpcode = kExprI64LoadMem;
            break;
        case 32:
            loadMemOpcode = kExprI64LoadMem32U;
            break;
        case 16:
            loadMemOpcode = kExprI64LoadMem16U;
            break;
        case 8:
            loadMemOpcode = kExprI64LoadMem8U;
            break;
        default:
            throw "!";
    }
    const kArgMemoryCell = 0; // target for atomic ops
    const kArgSequencePtr = 1; // address of sequence
    const kArgSeqenceLength = 2; // lenght of sequence
    const kArgWorkerId = 3; // id of this worker
    const kArgBitMask = 4; // mask to extract worker id from value
    const kLocalCurrentOffset = 5; // current position in sequence in bytes
    const kLocalExpectedValue = 6; // the value we are waiting for
    const kLocalNextValue = 7; // the value to write in the update
    let body = [
        // Turn sequence length to equivalent in bytes.
        kExprLocalGet, kArgSeqenceLength,
        kExprI32Const, size / 8,
        kExprI32Mul,
        kExprLocalSet, kArgSeqenceLength,
        // Outer block so we have something to jump for return.
        ...[kExprBlock, kWasmStmt,
            // Set counter to 0.
            kExprI32Const, 0,
            kExprLocalSet, kLocalCurrentOffset,
            // Outer loop until maxcount.
            ...[kExprLoop, kWasmStmt,
                // Find the next value to wait for.
                ...[kExprLoop, kWasmStmt,
                    // Check end of sequence.
                    kExprLocalGet, kLocalCurrentOffset,
                    kExprLocalGet, kArgSeqenceLength,
                    kExprI32Eq,
                    kExprBrIf, 2, // return
                    ...[kExprBlock, kWasmStmt,
                        // Load next value.
                        kExprLocalGet, kArgSequencePtr,
                        kExprLocalGet, kLocalCurrentOffset,
                        kExprI32Add,
                        loadMemOpcode, 0, 0,
                        // Mask off bits.
                        kExprLocalGet, kArgBitMask,
                        kExprI64UConvertI32,
                        kExprI64And,
                        // Compare with worker id.
                        kExprLocalGet, kArgWorkerId,
                        kExprI64UConvertI32,
                        kExprI64Eq,
                        kExprBrIf, 0,
                        // Not found, increment position.
                        kExprLocalGet, kLocalCurrentOffset,
                        kExprI32Const, size / 8,
                        kExprI32Add,
                        kExprLocalSet, kLocalCurrentOffset,
                        kExprBr, 1,
                        kExprEnd
                    ],
                    // Found, end loop.
                    kExprEnd
                ],
                // Load expected value to local.
                kExprLocalGet, kArgSequencePtr,
                kExprLocalGet, kLocalCurrentOffset,
                kExprI32Add,
                loadMemOpcode, 0, 0,
                kExprLocalSet, kLocalExpectedValue,
                // Load value after expected one.
                kExprLocalGet, kArgSequencePtr,
                kExprLocalGet, kLocalCurrentOffset,
                kExprI32Add,
                kExprI32Const, size / 8,
                kExprI32Add,
                loadMemOpcode, 0, 0,
                kExprLocalSet, kLocalNextValue,
                // Hammer on memory until value found.
                ...[kExprLoop, kWasmStmt,
                    // Load address.
                    kExprLocalGet, kArgMemoryCell,
                    // Load expected value.
                    kExprLocalGet, kLocalExpectedValue,
                    // Load updated value.
                    kExprLocalGet, kLocalNextValue,
                    // Try update.
                    kAtomicPrefix, compareExchangeOpcode, 0, 0,
                    // Load expected value.
                    kExprLocalGet, kLocalExpectedValue,
                    // Spin if not what expected.
                    kExprI64Ne,
                    kExprBrIf, 0,
                    kExprEnd
                ],
                // Next iteration of loop.
                kExprLocalGet, kLocalCurrentOffset,
                kExprI32Const, size / 8,
                kExprI32Add,
                kExprLocalSet, kLocalCurrentOffset,
                kExprBr, 0,
                kExprEnd
            ], // outer loop
            kExprEnd
        ], // the block
        kExprReturn
    ];
    builder.addFunction(functionName, makeSig([kWasmI32, kWasmI32, kWasmI32,
            kWasmI32, kWasmI32
        ], []))
        .addLocals({
            i32_count: 1, i64_count: 2
        })
        .addBody(body)
        .exportAs(functionName);
}

function generateSequence(typedarray, start, count) {
    let end = count + start;
    for (let i = start; i < end; i++) {
        typedarray[i] = Math.floor(Math.random() * 256);
    }
}

function spawnWorker(module, memory, address, sequence) {
    let workers = [];
    for (let i = 0; i < kNumberOfWorkers; i++) {
        let worker = new Worker(
            `onmessage = function(msg) {
                this.instance = new WebAssembly.Instance(msg.module,
                    {m: {imported_mem: msg.memory}});
                instance.exports.worker(msg.address, msg.sequence, msg.sequenceLength, msg.workerId,
                    msg.bitMask);
                postMessage({workerId: msg.workerId});
            }`,
            {type: 'string'}
        );
        workers.push(worker);
        worker.postMessage({
            module: module,
            memory: memory,
            address: address,
            sequence: sequence,
            sequenceLength: kSequenceLength,
            workerId: i,
            bitMask: kBitMask
        });
    }
    return workers;
}

function waitForWorkers(workers) {
    for (let worker of workers) {
        worker.getMessage();
        worker.terminate();
    }
}

function testOpcode(opcode, opcodeSize) {
    print("Testing I64AtomicCompareExchange" + opcodeSize);
    let builder = new WasmModuleBuilder();
    builder.addImportedMemory("m", "imported_mem", 0, 2, "shared");

    makeWorkerCodeForOpcode(opcode, opcodeSize, "worker", builder);

    let memory = new WebAssembly.Memory({
        initial: 2,
        maximum: 2,
        shared: true
    });
    let memoryView = new Uint8Array(memory.buffer);
    generateSequence(memoryView, kSequenceStartAddress, kSequenceLength * (opcodeSize / 8));

    let module = new WebAssembly.Module(builder.toBuffer());
    let workers = spawnWorker(module, memory, 0, kSequenceStartAddress);

    // Fire the workers off
    for (let i = opcodeSize / 8 - 1; i >= 0; i--) {
      memoryView[i] = memoryView[kSequenceStartAddress + i];
    }

    waitForWorkers(workers);

    print("DONE");
}

testOpcode(kExprI64AtomicCompareExchange, 64);
testOpcode(kExprI64AtomicCompareExchange32U, 32);
testOpcode(kExprI64AtomicCompareExchange16U, 16);
testOpcode(kExprI64AtomicCompareExchange8U, 8);
