"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.build = void 0;
const pushFileFilterAndCount = (filename, _paths, counts, filters) => {
    if (filters.every((filter) => filter(filename, false)))
        counts.files++;
};
const pushFileFilter = (filename, paths, _counts, filters) => {
    if (filters.every((filter) => filter(filename, false)))
        paths.push(filename);
};
const pushFileCount = (_filename, _paths, counts, _filters) => {
    counts.files++;
};
const pushFile = (filename, paths) => {
    paths.push(filename);
};
const empty = () => { };
function build(options) {
    const { excludeFiles, filters, onlyCounts } = options;
    if (excludeFiles)
        return empty;
    if (filters && filters.length) {
        return onlyCounts ? pushFileFilterAndCount : pushFileFilter;
    }
    else if (onlyCounts) {
        return pushFileCount;
    }
    else {
        return pushFile;
    }
}
exports.build = build;
