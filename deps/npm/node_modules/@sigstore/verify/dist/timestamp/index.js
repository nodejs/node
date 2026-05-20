"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.getTSATimestamp = getTSATimestamp;
exports.getTLogTimestamp = getTLogTimestamp;
const tsa_1 = require("./tsa");
function getTSATimestamp(timestamp, data, timestampAuthorities) {
    (0, tsa_1.verifyRFC3161Timestamp)(timestamp, data, timestampAuthorities);
    return {
        type: 'timestamp-authority',
        logID: timestamp.signerSerialNumber,
        timestamp: timestamp.signingTime,
    };
}
function getTLogTimestamp(entry) {
    return {
        type: 'transparency-log',
        logID: entry.logId.keyId,
        timestamp: new Date(Number(entry.integratedTime) * 1000),
    };
}
