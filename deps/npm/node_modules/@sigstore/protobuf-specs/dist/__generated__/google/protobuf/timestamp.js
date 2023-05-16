"use strict";
/* eslint-disable */
Object.defineProperty(exports, "__esModule", { value: true });
exports.Timestamp = void 0;
function createBaseTimestamp() {
    return { seconds: "0", nanos: 0 };
}
exports.Timestamp = {
    fromJSON(object) {
        return {
            seconds: isSet(object.seconds) ? String(object.seconds) : "0",
            nanos: isSet(object.nanos) ? Number(object.nanos) : 0,
        };
    },
    toJSON(message) {
        const obj = {};
        message.seconds !== undefined && (obj.seconds = message.seconds);
        message.nanos !== undefined && (obj.nanos = Math.round(message.nanos));
        return obj;
    },
};
function isSet(value) {
    return value !== null && value !== undefined;
}
