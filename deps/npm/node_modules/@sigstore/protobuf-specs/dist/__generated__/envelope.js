"use strict";
/* eslint-disable */
Object.defineProperty(exports, "__esModule", { value: true });
exports.Signature = exports.Envelope = void 0;
function createBaseEnvelope() {
    return { payload: Buffer.alloc(0), payloadType: "", signatures: [] };
}
exports.Envelope = {
    fromJSON(object) {
        return {
            payload: isSet(object.payload) ? Buffer.from(bytesFromBase64(object.payload)) : Buffer.alloc(0),
            payloadType: isSet(object.payloadType) ? String(object.payloadType) : "",
            signatures: Array.isArray(object?.signatures) ? object.signatures.map((e) => exports.Signature.fromJSON(e)) : [],
        };
    },
    toJSON(message) {
        const obj = {};
        message.payload !== undefined &&
            (obj.payload = base64FromBytes(message.payload !== undefined ? message.payload : Buffer.alloc(0)));
        message.payloadType !== undefined && (obj.payloadType = message.payloadType);
        if (message.signatures) {
            obj.signatures = message.signatures.map((e) => e ? exports.Signature.toJSON(e) : undefined);
        }
        else {
            obj.signatures = [];
        }
        return obj;
    },
};
function createBaseSignature() {
    return { sig: Buffer.alloc(0), keyid: "" };
}
exports.Signature = {
    fromJSON(object) {
        return {
            sig: isSet(object.sig) ? Buffer.from(bytesFromBase64(object.sig)) : Buffer.alloc(0),
            keyid: isSet(object.keyid) ? String(object.keyid) : "",
        };
    },
    toJSON(message) {
        const obj = {};
        message.sig !== undefined && (obj.sig = base64FromBytes(message.sig !== undefined ? message.sig : Buffer.alloc(0)));
        message.keyid !== undefined && (obj.keyid = message.keyid);
        return obj;
    },
};
var tsProtoGlobalThis = (() => {
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
    if (tsProtoGlobalThis.Buffer) {
        return Uint8Array.from(tsProtoGlobalThis.Buffer.from(b64, "base64"));
    }
    else {
        const bin = tsProtoGlobalThis.atob(b64);
        const arr = new Uint8Array(bin.length);
        for (let i = 0; i < bin.length; ++i) {
            arr[i] = bin.charCodeAt(i);
        }
        return arr;
    }
}
function base64FromBytes(arr) {
    if (tsProtoGlobalThis.Buffer) {
        return tsProtoGlobalThis.Buffer.from(arr).toString("base64");
    }
    else {
        const bin = [];
        arr.forEach((byte) => {
            bin.push(String.fromCharCode(byte));
        });
        return tsProtoGlobalThis.btoa(bin.join(""));
    }
}
function isSet(value) {
    return value !== null && value !== undefined;
}
