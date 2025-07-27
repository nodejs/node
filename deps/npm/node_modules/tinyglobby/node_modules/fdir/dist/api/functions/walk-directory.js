"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.build = void 0;
const fs_1 = __importDefault(require("fs"));
const readdirOpts = { withFileTypes: true };
const walkAsync = (state, crawlPath, directoryPath, currentDepth, callback) => {
    state.queue.enqueue();
    if (currentDepth < 0)
        return state.queue.dequeue(null, state);
    state.visited.push(crawlPath);
    state.counts.directories++;
    // Perf: Node >= 10 introduced withFileTypes that helps us
    // skip an extra fs.stat call.
    fs_1.default.readdir(crawlPath || ".", readdirOpts, (error, entries = []) => {
        callback(entries, directoryPath, currentDepth);
        state.queue.dequeue(state.options.suppressErrors ? null : error, state);
    });
};
const walkSync = (state, crawlPath, directoryPath, currentDepth, callback) => {
    if (currentDepth < 0)
        return;
    state.visited.push(crawlPath);
    state.counts.directories++;
    let entries = [];
    try {
        entries = fs_1.default.readdirSync(crawlPath || ".", readdirOpts);
    }
    catch (e) {
        if (!state.options.suppressErrors)
            throw e;
    }
    callback(entries, directoryPath, currentDepth);
};
function build(isSynchronous) {
    return isSynchronous ? walkSync : walkAsync;
}
exports.build = build;
