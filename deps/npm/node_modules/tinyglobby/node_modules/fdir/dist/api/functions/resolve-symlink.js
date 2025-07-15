"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.build = void 0;
const fs_1 = __importDefault(require("fs"));
const path_1 = require("path");
const resolveSymlinksAsync = function (path, state, callback) {
    const { queue, options: { suppressErrors }, } = state;
    queue.enqueue();
    fs_1.default.realpath(path, (error, resolvedPath) => {
        if (error)
            return queue.dequeue(suppressErrors ? null : error, state);
        fs_1.default.stat(resolvedPath, (error, stat) => {
            if (error)
                return queue.dequeue(suppressErrors ? null : error, state);
            if (stat.isDirectory() && isRecursive(path, resolvedPath, state))
                return queue.dequeue(null, state);
            callback(stat, resolvedPath);
            queue.dequeue(null, state);
        });
    });
};
const resolveSymlinks = function (path, state, callback) {
    const { queue, options: { suppressErrors }, } = state;
    queue.enqueue();
    try {
        const resolvedPath = fs_1.default.realpathSync(path);
        const stat = fs_1.default.statSync(resolvedPath);
        if (stat.isDirectory() && isRecursive(path, resolvedPath, state))
            return;
        callback(stat, resolvedPath);
    }
    catch (e) {
        if (!suppressErrors)
            throw e;
    }
};
function build(options, isSynchronous) {
    if (!options.resolveSymlinks || options.excludeSymlinks)
        return null;
    return isSynchronous ? resolveSymlinks : resolveSymlinksAsync;
}
exports.build = build;
function isRecursive(path, resolved, state) {
    if (state.options.useRealPaths)
        return isRecursiveUsingRealPaths(resolved, state);
    let parent = (0, path_1.dirname)(path);
    let depth = 1;
    while (parent !== state.root && depth < 2) {
        const resolvedPath = state.symlinks.get(parent);
        const isSameRoot = !!resolvedPath &&
            (resolvedPath === resolved ||
                resolvedPath.startsWith(resolved) ||
                resolved.startsWith(resolvedPath));
        if (isSameRoot)
            depth++;
        else
            parent = (0, path_1.dirname)(parent);
    }
    state.symlinks.set(path, resolved);
    return depth > 1;
}
function isRecursiveUsingRealPaths(resolved, state) {
    return state.visited.includes(resolved + state.options.pathSeparator);
}
