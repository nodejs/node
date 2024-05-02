"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.join = void 0;
const url_1 = require("url");
function join(base, path) {
    return new url_1.URL(ensureTrailingSlash(base) + removeLeadingSlash(path)).toString();
}
exports.join = join;
function ensureTrailingSlash(path) {
    return path.endsWith('/') ? path : path + '/';
}
function removeLeadingSlash(path) {
    return path.startsWith('/') ? path.slice(1) : path;
}
