// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a copy from:
// https://github.com/googleprojectzero/fuzzilli/blob/main/Sources/Fuzzilli/Profiles/V8SandboxProfile.swift

// The following functions corrupt a given object in a deterministic fashion (based on the provided seed and path) and log the steps being performed.

function getSandboxCorruptionHelpers() {
    // In general, memory contents are represented as (unsigned) BigInts, everything else (addresses, offsets, etc.) are Numbers.

    function assert(c) {
        if (!c) {
            throw new Error("Assertion in the in-sandbox-corruption API failed!");
        }
    }

    const kHeapObjectTag = 0x1n;
    // V8 uses the two lowest bits as tag bits: 0x1 to indicate HeapObject vs Smi and 0x2 to indicate a weak reference.
    const kHeapObjectTagMask = 0x3n;
    // Offsets should be a multiple of 4, as that's the typical field size.
    const kOffsetAlignmentMask = ~0x3;

    const builtins = Sandbox.getBuiltinNames();
    assert(builtins.length > 0);

    const Step = {
        NEIGHBOR: 0,
        POINTER: 1,
    };

    // Helper class for accessing in-sandbox memory.
    class Memory {
        constructor() {
            let buffer = new Sandbox.MemoryView(0, 0x100000000);
            this.dataView = new DataView(buffer);
            this.taggedView = new Uint32Array(buffer);
        }

        read(addr, numBits) {
            switch (numBits) {
                case 8: return BigInt(this.dataView.getUint8(addr));
                case 16: return BigInt(this.dataView.getUint16(addr, true));
                case 32: return BigInt(this.dataView.getUint32(addr, true));
            }
        }

        write(addr, value, numBits) {
            switch (numBits) {
                case 8: this.dataView.setUint8(addr, Number(value)); break;
                case 16: this.dataView.setUint16(addr, Number(value), true); break;
                case 32: this.dataView.setUint32(addr, Number(value), true); break;
            }
        }

        copyTagged(source, destination, size) {
            assert(size % 4 == 0);
            let toIndex = destination / 4;
            let startIndex = source / 4;
            let endIndex = (source + size) / 4;
            this.taggedView.copyWithin(toIndex, startIndex, endIndex);
        }
    }
    let memory = new Memory;

    // A worker thread that corrupts memory from the background.
    //
    // The main thread can post messages to this worker which contain
    // effectively (address, valueA, valueB) triples. This worker
    // will then permanently flip all the given addresses between
    // valueA and valueB. This for example makes it possible to find
    // double-fetch issues and similar bugs.
    function workerFunc() {
        let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
        let work = [];
        let iteration = 0;

        onmessage = function(e) {
            if (work.length == 0) {
                // Time to start working.
                setTimeout(doWork);
            }
            work.push(e.data);
        }

        function corrupt(address, value, size) {
            switch (size) {
                case 8:
                    memory.setUint8(address, value);
                    break;
                case 16:
                    memory.setUint16(address, value, true);
                    break;
                case 32:
                    memory.setUint32(address, value, true);
                    break;
            }
        }

        function doWork() {
            iteration++;
            for (let item of work) {
                let value = (iteration % 2) == 0 ? item.valueA : item.valueB;
                corrupt(item.address, value, item.size);
            }
            // Schedule the next round of work.
            setTimeout(doWork);
        }
    }
    if (typeof globalThis.memory_corruption_worker === 'undefined') {
        // Define as non-configurable and non-enumerable property.
        let worker = new Worker(workerFunc, {type: 'function'});
        Object.defineProperty(globalThis, 'memory_corruption_worker', {value: worker});
    }


    // Helper function to deterministically find a random neighbor object.
    //
    // This logic is designed to deal with a (somewhat) non-deterministic heap layout to ensure that test cases are reproducible.
    // In practice, it should most of the time find the same neighbor object if (a) that object is always allocated after the
    // start object, (b) is within the first N (currently 100) objects, and (c) is always the first neighbor of its instance type.
    //
    // This is achieved by iterating over the heap starting from the start object and computing a simple 16-bit hash value for each
    // object. At the end, we select the first object whose hash is closest to a random 16-bit hash query.
    // Note that we always take the first object if there are multiple objects with the same instance type.
    // For finding later neighbors, we rely on the traversal path containing multiple Step.NEIGHBOR entries.
    function findRandomNeighborObject(addr, hashQuery) {
        const N = 100;
        const kUint16Max = 0xffff;
        const kUnknownInstanceId = kUint16Max;

        // Simple hash function for 16-bit unsigned integers. See https://github.com/skeeto/hash-prospector
        function hash16(x) {
            assert(x >= 0 && x <= kUint16Max);
            x ^= x >> 8;
            x = (x * 0x88B5) & 0xffff;
            x ^= x >> 7;
            x = (x * 0xdB2d) & 0xffff;
            x ^= x >> 9;
            return x;
        }

        hashQuery = hashQuery & 0xffff;
        let currentWinner = addr;
        let currentBest = kUint16Max;

        for (let i = 0; i < N; i++) {
            addr += Sandbox.getSizeOfObjectAt(addr);
            let typeId = Sandbox.getInstanceTypeIdOfObjectAt(addr);
            if (typeId == kUnknownInstanceId) {
                break;
            }
            let hash = hash16(typeId);
            let score = Math.abs(hash - hashQuery);
            if (score < currentBest) {
                currentBest = score;
                currentWinner = addr;
            }
        }

        return currentWinner;
    }

    // Helper function to create a copy of the object at the given address and return the address of the copy.
    // This is for example useful when we would like to corrupt a read-only object: in that case, we can then instead
    // create a copy of the read-only object, install that into whichever object references is, then corrupt the copy.
    function copyObjectAt(source) {
        let objectSize = Sandbox.getSizeOfObjectAt(source);
        // Simple way to get a placeholder object that's large enough: create a sequential string.
        // TODO(saelo): maybe add a method to the sandbox api to construct an object of the appropriate size.
        let placeholder = Array(objectSize).fill("a").join("");
        let destination = Sandbox.getAddressOf(placeholder);
        memory.copyTagged(source, destination, objectSize);
        return destination;
    }

    function getRandomAlignedOffset(addr, offsetSeed) {
        let objectSize = Sandbox.getSizeOfObjectAt(addr);
        return (offsetSeed % objectSize) & kOffsetAlignmentMask;
    }

    function getBaseAddress(obj) {
        try {
            if (!Sandbox.isWritable(obj)) return null;
            return Sandbox.getAddressOf(obj);
        } catch (e) {
            // Presumably, |obj| is a Smi, not a HeapObject.
            return null;
        }
    }

    function prepareDataCorruptionContext(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset) {
        let baseAddr = getBaseAddress(obj);
        if (!baseAddr) return null;

        let addr = evaluateTraversalPath(baseAddr, path);
        if (!addr) return null;

        let offset = getRandomAlignedOffset(addr, offsetSeed);
        offset += subFieldOffset;

        let oldValue = memory.read(addr + offset, numBitsToCorrupt);
        return { addr, offset, oldValue, finalizeDataCorruption };
    }

    function finalizeDataCorruption(addr, offset, oldValue, newValue, numBitsToCorrupt, typeString) {
        assert(oldValue >= 0 && oldValue < (1n << BigInt(numBitsToCorrupt)));
        assert(newValue >= 0 && newValue < (1n << BigInt(numBitsToCorrupt)));

        memory.write(addr + offset, newValue, numBitsToCorrupt);
        print("  Corrupted " + numBitsToCorrupt + "-bit field (" + typeString + ") at offset " + offset + ". Old value: 0x" + oldValue.toString(16) + ", new value: 0x" + newValue.toString(16));
    }

    // The path argument is an array of [Step, Value] tuples.
    // If Step === Step.NEIGHBOR, Value is the exact 16-bit hash query.
    // If Step === Step.POINTER, Value is a random UInt32 seed used to calculate an aligned offset.
    function evaluateTraversalPath(addr, path) {
        let instanceType = Sandbox.getInstanceTypeOfObjectAt(addr);
        print("Corrupting memory starting from object at 0x" + addr.toString(16) + " of type " + instanceType);

        for (let [stepType, seedValue] of path) {
            if (!Sandbox.isWritableObjectAt(addr)) {
                print("  Not corrupting read-only object. Bailing out.");
                return null;
            }

            switch (stepType) {
                case Step.NEIGHBOR: {
                    let oldAddr = addr;
                    addr = findRandomNeighborObject(addr, seedValue);
                    print("  Jumping to neighboring object at offset " + (addr - oldAddr));
                    break;
                }
                case Step.POINTER: {
                    let offset = getRandomAlignedOffset(addr, seedValue);
                    let oldValue = memory.read(addr + offset, 32);

                    // If the selected offset doesn't contain a valid pointer, we break out
                    // of the traversal loop but still corrupt the current (valid) object.
                    let isLikelyPointer = (oldValue & kHeapObjectTag) == kHeapObjectTag;
                    if (!isLikelyPointer) {
                        break;
                    }

                    let newAddr = Number(oldValue & ~kHeapObjectTagMask);
                    if (!Sandbox.isValidObjectAt(newAddr)) {
                        break;
                    }

                    print("  Following pointer at offset " + offset + " to object at 0x" + newAddr.toString(16));

                    if (!Sandbox.isWritableObjectAt(newAddr)) {
                        newAddr = copyObjectAt(newAddr);
                        memory.write(addr + offset, BigInt(newAddr) | kHeapObjectTag, 32);
                        print("  Referenced object is in read-only memory. Created and linked a writable copy at 0x" + newAddr.toString(16));
                    }
                    addr = newAddr;
                    break;
                }
            }
        }
        return Sandbox.isWritableObjectAt(addr) ? addr : null;
    }

    return {
        builtins, getBaseAddress, evaluateTraversalPath, prepareDataCorruptionContext
    };
}

function corruptDataWithBitflip(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset, bitPosition) {
    let { addr, offset, oldValue, finalizeDataCorruption } = getSandboxCorruptionHelpers().prepareDataCorruptionContext(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset) || {};
    if (!addr) return;

    let newValue = oldValue ^ (1n << BigInt(bitPosition));
    finalizeDataCorruption(addr, offset, oldValue, newValue, numBitsToCorrupt, "Bitflip");
}

function corruptDataWithIncrement(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset, incrementValue) {
    let { addr, offset, oldValue, finalizeDataCorruption } = getSandboxCorruptionHelpers().prepareDataCorruptionContext(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset) || {};
    if (!addr) return;

    let newValue = (oldValue + incrementValue) & ((1n << BigInt(numBitsToCorrupt)) - 1n);
    finalizeDataCorruption(addr, offset, oldValue, newValue, numBitsToCorrupt, "Increment");
}

function corruptDataWithReplace(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset, replaceValue) {
    let { addr, offset, oldValue, finalizeDataCorruption } = getSandboxCorruptionHelpers().prepareDataCorruptionContext(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset) || {};
    if (!addr) return;

    let newValue = replaceValue;
    finalizeDataCorruption(addr, offset, oldValue, newValue, numBitsToCorrupt, "Replace");
}

function corruptWithWorker(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset, bitPosition) {
    let { addr, offset, oldValue } = getSandboxCorruptionHelpers().prepareDataCorruptionContext(obj, path, offsetSeed, numBitsToCorrupt, subFieldOffset) || {};
    if (!addr) return;

    let newValue = oldValue ^ (1n << BigInt(bitPosition));

    globalThis.memory_corruption_worker.postMessage({
        address: addr + offset, valueA: Number(oldValue), valueB: Number(newValue), size: numBitsToCorrupt
    });

    print("  Started background worker to continuously flip " + numBitsToCorrupt + "-bit field at offset " + offset + " between 0x" + oldValue.toString(16) + " and 0x" + newValue.toString(16));
}

function corruptFunction(obj, path, builtinSeed) {
    let { builtins, getBaseAddress, evaluateTraversalPath } = getSandboxCorruptionHelpers();
    let baseAddr = getBaseAddress(obj);
    if (!baseAddr) return;
    let addr = evaluateTraversalPath(baseAddr, path);
    if (!addr) return;

    let instanceTypeId = Sandbox.getInstanceTypeIdOfObjectAt(addr);
    if (instanceTypeId === Sandbox.getInstanceTypeIdFor("JS_FUNCTION_TYPE")) {
        let targetObj = Sandbox.getObjectAt(addr);
        let builtinId = builtinSeed % builtins.length;
        try {
            Sandbox.setFunctionCodeToBuiltin(targetObj, builtinId);
            print("  Hijacked JSFunction code pointer! Swapped with builtin: " + builtins[builtinId]);
        } catch(e) {}
    }
}
