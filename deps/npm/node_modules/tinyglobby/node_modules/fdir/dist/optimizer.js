"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.findMaxDepth = exports.findDirectoryPatterns = exports.findCommonRoots = void 0;
// Glob Optimizations:
// 1. Find common roots and only iterate on them
//    For example:
//      1. "node_modules/**/*.ts" only requires us to search in node_modules
//          folder.
//      2. Similarly, multiple glob patterns can have common deterministic roots
//         The optimizer's job is to find these roots and only crawl them.
//      3. If any of the glob patterns have a globstar i.e. **/ in them, we
//         should bail out.
// 2. Find out if glob is requesting only directories
// 3. Find maximum depth requested
// 4. If glob contains a root that doesn't exist, bail out
const braces_1 = require("braces");
const glob_parent_1 = __importDefault(require("glob-parent"));
function findCommonRoots(patterns) {
    const allRoots = new Set();
    patterns = patterns.map((p) => (p.includes("{") ? (0, braces_1.expand)(p) : p)).flat();
    for (const pattern of patterns) {
        const parent = (0, glob_parent_1.default)(pattern);
        if (parent === ".")
            return [];
        allRoots.add(parent);
    }
    return Array.from(allRoots.values()).filter((root) => {
        for (const r of allRoots) {
            if (r === root)
                continue;
            if (root.startsWith(r))
                return false;
        }
        return true;
    });
}
exports.findCommonRoots = findCommonRoots;
function findDirectoryPatterns(patterns) {
    return patterns.filter((p) => p.endsWith("/"));
}
exports.findDirectoryPatterns = findDirectoryPatterns;
function findMaxDepth(patterns) {
    const isGlobstar = patterns.some((p) => p.includes("**/") || p.includes("/**") || p === "**");
    if (isGlobstar)
        return false;
    const maxDepth = patterns.reduce((depth, p) => {
        return Math.max(depth, p.split("/").filter(Boolean).length);
    }, 0);
    return maxDepth;
}
exports.findMaxDepth = findMaxDepth;
