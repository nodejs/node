"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.build = void 0;
function pushDirectoryWithRelativePath(root) {
    return function (directoryPath, paths) {
        paths.push(directoryPath.substring(root.length) || ".");
    };
}
function pushDirectoryFilterWithRelativePath(root) {
    return function (directoryPath, paths, filters) {
        const relativePath = directoryPath.substring(root.length) || ".";
        if (filters.every((filter) => filter(relativePath, true))) {
            paths.push(relativePath);
        }
    };
}
const pushDirectory = (directoryPath, paths) => {
    paths.push(directoryPath || ".");
};
const pushDirectoryFilter = (directoryPath, paths, filters) => {
    const path = directoryPath || ".";
    if (filters.every((filter) => filter(path, true))) {
        paths.push(path);
    }
};
const empty = () => { };
function build(root, options) {
    const { includeDirs, filters, relativePaths } = options;
    if (!includeDirs)
        return empty;
    if (relativePaths)
        return filters && filters.length
            ? pushDirectoryFilterWithRelativePath(root)
            : pushDirectoryWithRelativePath(root);
    return filters && filters.length ? pushDirectoryFilter : pushDirectory;
}
exports.build = build;
