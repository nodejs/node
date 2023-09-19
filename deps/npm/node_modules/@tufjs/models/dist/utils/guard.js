"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.isObjectRecord = exports.isStringRecord = exports.isObjectArray = exports.isStringArray = exports.isObject = exports.isDefined = void 0;
function isDefined(val) {
    return val !== undefined;
}
exports.isDefined = isDefined;
function isObject(value) {
    return typeof value === 'object' && value !== null;
}
exports.isObject = isObject;
function isStringArray(value) {
    return Array.isArray(value) && value.every((v) => typeof v === 'string');
}
exports.isStringArray = isStringArray;
function isObjectArray(value) {
    return Array.isArray(value) && value.every(isObject);
}
exports.isObjectArray = isObjectArray;
function isStringRecord(value) {
    return (typeof value === 'object' &&
        value !== null &&
        Object.keys(value).every((k) => typeof k === 'string') &&
        Object.values(value).every((v) => typeof v === 'string'));
}
exports.isStringRecord = isStringRecord;
function isObjectRecord(value) {
    return (typeof value === 'object' &&
        value !== null &&
        Object.keys(value).every((k) => typeof k === 'string') &&
        Object.values(value).every((v) => typeof v === 'object' && v !== null));
}
exports.isObjectRecord = isObjectRecord;
