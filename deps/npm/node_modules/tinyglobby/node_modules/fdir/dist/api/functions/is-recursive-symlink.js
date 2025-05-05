"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.isRecursive = exports.isRecursiveAsync = void 0;
const path_1 = require("path");
const fs_1 = require("fs");
const isRecursiveAsync = (state, path, resolved, callback) => {
    if (state.options.useRealPaths)
        return callback(state.visited.has(resolved + state.options.pathSeparator));
    let parent = (0, path_1.dirname)(path);
    if (parent + state.options.pathSeparator === state.root || parent === path)
        return callback(false);
    if (state.symlinks.get(parent) === resolved)
        return callback(true);
    (0, fs_1.readlink)(parent, (error, resolvedParent) => {
        if (error)
            return (0, exports.isRecursiveAsync)(state, parent, resolved, callback);
        callback(resolvedParent === resolved);
    });
};
exports.isRecursiveAsync = isRecursiveAsync;
function isRecursive(state, path, resolved) {
    if (state.options.useRealPaths)
        return state.visited.has(resolved + state.options.pathSeparator);
    let parent = (0, path_1.dirname)(path);
    if (parent + state.options.pathSeparator === state.root || parent === path)
        return false;
    try {
        const resolvedParent = state.symlinks.get(parent) || (0, fs_1.readlinkSync)(parent);
        return resolvedParent === resolved;
    }
    catch (e) {
        return isRecursive(state, parent, resolved);
    }
}
exports.isRecursive = isRecursive;
