"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifySignature = void 0;
const canonical_json_1 = require("@tufjs/canonical-json");
const crypto_1 = __importDefault(require("crypto"));
const verifySignature = (metaDataSignedData, key, signature) => {
    const canonicalData = Buffer.from((0, canonical_json_1.canonicalize)(metaDataSignedData));
    return crypto_1.default.verify(undefined, canonicalData, key, Buffer.from(signature, 'hex'));
};
exports.verifySignature = verifySignature;
