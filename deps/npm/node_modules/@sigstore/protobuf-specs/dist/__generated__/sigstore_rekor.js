"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TransparencyLogEntry = exports.InclusionPromise = exports.InclusionProof = exports.Checkpoint = exports.KindVersion = void 0;
/* eslint-disable */
const sigstore_common_1 = require("./sigstore_common");
function createBaseKindVersion() {
    return { kind: "", version: "" };
}
exports.KindVersion = {
    fromJSON(object) {
        return {
            kind: isSet(object.kind) ? String(object.kind) : "",
            version: isSet(object.version) ? String(object.version) : "",
        };
    },
    toJSON(message) {
        const obj = {};
        message.kind !== undefined && (obj.kind = message.kind);
        message.version !== undefined && (obj.version = message.version);
        return obj;
    },
};
function createBaseCheckpoint() {
    return { envelope: "" };
}
exports.Checkpoint = {
    fromJSON(object) {
        return { envelope: isSet(object.envelope) ? String(object.envelope) : "" };
    },
    toJSON(message) {
        const obj = {};
        message.envelope !== undefined && (obj.envelope = message.envelope);
        return obj;
    },
};
function createBaseInclusionProof() {
    return { logIndex: "0", rootHash: Buffer.alloc(0), treeSize: "0", hashes: [], checkpoint: undefined };
}
exports.InclusionProof = {
    fromJSON(object) {
        return {
            logIndex: isSet(object.logIndex) ? String(object.logIndex) : "0",
            rootHash: isSet(object.rootHash) ? Buffer.from(bytesFromBase64(object.rootHash)) : Buffer.alloc(0),
            treeSize: isSet(object.treeSize) ? String(object.treeSize) : "0",
            hashes: Array.isArray(object?.hashes) ? object.hashes.map((e) => Buffer.from(bytesFromBase64(e))) : [],
            checkpoint: isSet(object.checkpoint) ? exports.Checkpoint.fromJSON(object.checkpoint) : undefined,
        };
    },
    toJSON(message) {
        const obj = {};
        message.logIndex !== undefined && (obj.logIndex = message.logIndex);
        message.rootHash !== undefined &&
            (obj.rootHash = base64FromBytes(message.rootHash !== undefined ? message.rootHash : Buffer.alloc(0)));
        message.treeSize !== undefined && (obj.treeSize = message.treeSize);
        if (message.hashes) {
            obj.hashes = message.hashes.map((e) => base64FromBytes(e !== undefined ? e : Buffer.alloc(0)));
        }
        else {
            obj.hashes = [];
        }
        message.checkpoint !== undefined &&
            (obj.checkpoint = message.checkpoint ? exports.Checkpoint.toJSON(message.checkpoint) : undefined);
        return obj;
    },
};
function createBaseInclusionPromise() {
    return { signedEntryTimestamp: Buffer.alloc(0) };
}
exports.InclusionPromise = {
    fromJSON(object) {
        return {
            signedEntryTimestamp: isSet(object.signedEntryTimestamp)
                ? Buffer.from(bytesFromBase64(object.signedEntryTimestamp))
                : Buffer.alloc(0),
        };
    },
    toJSON(message) {
        const obj = {};
        message.signedEntryTimestamp !== undefined &&
            (obj.signedEntryTimestamp = base64FromBytes(message.signedEntryTimestamp !== undefined ? message.signedEntryTimestamp : Buffer.alloc(0)));
        return obj;
    },
};
function createBaseTransparencyLogEntry() {
    return {
        logIndex: "0",
        logId: undefined,
        kindVersion: undefined,
        integratedTime: "0",
        inclusionPromise: undefined,
        inclusionProof: undefined,
        canonicalizedBody: Buffer.alloc(0),
    };
}
exports.TransparencyLogEntry = {
    fromJSON(object) {
        return {
            logIndex: isSet(object.logIndex) ? String(object.logIndex) : "0",
            logId: isSet(object.logId) ? sigstore_common_1.LogId.fromJSON(object.logId) : undefined,
            kindVersion: isSet(object.kindVersion) ? exports.KindVersion.fromJSON(object.kindVersion) : undefined,
            integratedTime: isSet(object.integratedTime) ? String(object.integratedTime) : "0",
            inclusionPromise: isSet(object.inclusionPromise) ? exports.InclusionPromise.fromJSON(object.inclusionPromise) : undefined,
            inclusionProof: isSet(object.inclusionProof) ? exports.InclusionProof.fromJSON(object.inclusionProof) : undefined,
            canonicalizedBody: isSet(object.canonicalizedBody)
                ? Buffer.from(bytesFromBase64(object.canonicalizedBody))
                : Buffer.alloc(0),
        };
    },
    toJSON(message) {
        const obj = {};
        message.logIndex !== undefined && (obj.logIndex = message.logIndex);
        message.logId !== undefined && (obj.logId = message.logId ? sigstore_common_1.LogId.toJSON(message.logId) : undefined);
        message.kindVersion !== undefined &&
            (obj.kindVersion = message.kindVersion ? exports.KindVersion.toJSON(message.kindVersion) : undefined);
        message.integratedTime !== undefined && (obj.integratedTime = message.integratedTime);
        message.inclusionPromise !== undefined &&
            (obj.inclusionPromise = message.inclusionPromise ? exports.InclusionPromise.toJSON(message.inclusionPromise) : undefined);
        message.inclusionProof !== undefined &&
            (obj.inclusionProof = message.inclusionProof ? exports.InclusionProof.toJSON(message.inclusionProof) : undefined);
        message.canonicalizedBody !== undefined &&
            (obj.canonicalizedBody = base64FromBytes(message.canonicalizedBody !== undefined ? message.canonicalizedBody : Buffer.alloc(0)));
        return obj;
    },
};
var globalThis = (() => {
    if (typeof globalThis !== "undefined") {
        return globalThis;
    }
    if (typeof self !== "undefined") {
        return self;
    }
    if (typeof window !== "undefined") {
        return window;
    }
    if (typeof global !== "undefined") {
        return global;
    }
    throw "Unable to locate global object";
})();
function bytesFromBase64(b64) {
    if (globalThis.Buffer) {
        return Uint8Array.from(globalThis.Buffer.from(b64, "base64"));
    }
    else {
        const bin = globalThis.atob(b64);
        const arr = new Uint8Array(bin.length);
        for (let i = 0; i < bin.length; ++i) {
            arr[i] = bin.charCodeAt(i);
        }
        return arr;
    }
}
function base64FromBytes(arr) {
    if (globalThis.Buffer) {
        return globalThis.Buffer.from(arr).toString("base64");
    }
    else {
        const bin = [];
        arr.forEach((byte) => {
            bin.push(String.fromCharCode(byte));
        });
        return globalThis.btoa(bin.join(""));
    }
}
function isSet(value) {
    return value !== null && value !== undefined;
}
