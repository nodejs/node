"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.build = void 0;
const onlyCountsSync = (state) => {
    return state.counts;
};
const groupsSync = (state) => {
    return state.groups;
};
const defaultSync = (state) => {
    return state.paths;
};
const limitFilesSync = (state) => {
    return state.paths.slice(0, state.options.maxFiles);
};
const onlyCountsAsync = (state, error, callback) => {
    report(error, callback, state.counts, state.options.suppressErrors);
    return null;
};
const defaultAsync = (state, error, callback) => {
    report(error, callback, state.paths, state.options.suppressErrors);
    return null;
};
const limitFilesAsync = (state, error, callback) => {
    report(error, callback, state.paths.slice(0, state.options.maxFiles), state.options.suppressErrors);
    return null;
};
const groupsAsync = (state, error, callback) => {
    report(error, callback, state.groups, state.options.suppressErrors);
    return null;
};
function report(error, callback, output, suppressErrors) {
    if (error && !suppressErrors)
        callback(error, output);
    else
        callback(null, output);
}
function build(options, isSynchronous) {
    const { onlyCounts, group, maxFiles } = options;
    if (onlyCounts)
        return isSynchronous
            ? onlyCountsSync
            : onlyCountsAsync;
    else if (group)
        return isSynchronous
            ? groupsSync
            : groupsAsync;
    else if (maxFiles)
        return isSynchronous
            ? limitFilesSync
            : limitFilesAsync;
    else
        return isSynchronous
            ? defaultSync
            : defaultAsync;
}
exports.build = build;
