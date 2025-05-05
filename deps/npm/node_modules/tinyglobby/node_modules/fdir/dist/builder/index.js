"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Builder = void 0;
const path_1 = require("path");
const api_builder_1 = require("./api-builder");
var pm = null;
/* c8 ignore next 6 */
try {
    require.resolve("picomatch");
    pm = require("picomatch");
}
catch (_e) {
    // do nothing
}
class Builder {
    globCache = {};
    options = {
        maxDepth: Infinity,
        suppressErrors: true,
        pathSeparator: path_1.sep,
        filters: [],
    };
    globFunction;
    constructor(options) {
        this.options = { ...this.options, ...options };
        this.globFunction = this.options.globFunction;
    }
    group() {
        this.options.group = true;
        return this;
    }
    withPathSeparator(separator) {
        this.options.pathSeparator = separator;
        return this;
    }
    withBasePath() {
        this.options.includeBasePath = true;
        return this;
    }
    withRelativePaths() {
        this.options.relativePaths = true;
        return this;
    }
    withDirs() {
        this.options.includeDirs = true;
        return this;
    }
    withMaxDepth(depth) {
        this.options.maxDepth = depth;
        return this;
    }
    withMaxFiles(limit) {
        this.options.maxFiles = limit;
        return this;
    }
    withFullPaths() {
        this.options.resolvePaths = true;
        this.options.includeBasePath = true;
        return this;
    }
    withErrors() {
        this.options.suppressErrors = false;
        return this;
    }
    withSymlinks({ resolvePaths = true } = {}) {
        this.options.resolveSymlinks = true;
        this.options.useRealPaths = resolvePaths;
        return this.withFullPaths();
    }
    withAbortSignal(signal) {
        this.options.signal = signal;
        return this;
    }
    normalize() {
        this.options.normalizePath = true;
        return this;
    }
    filter(predicate) {
        this.options.filters.push(predicate);
        return this;
    }
    onlyDirs() {
        this.options.excludeFiles = true;
        this.options.includeDirs = true;
        return this;
    }
    exclude(predicate) {
        this.options.exclude = predicate;
        return this;
    }
    onlyCounts() {
        this.options.onlyCounts = true;
        return this;
    }
    crawl(root) {
        return new api_builder_1.APIBuilder(root || ".", this.options);
    }
    withGlobFunction(fn) {
        // cast this since we don't have the new type params yet
        this.globFunction = fn;
        return this;
    }
    /**
     * @deprecated Pass options using the constructor instead:
     * ```ts
     * new fdir(options).crawl("/path/to/root");
     * ```
     * This method will be removed in v7.0
     */
    /* c8 ignore next 4 */
    crawlWithOptions(root, options) {
        this.options = { ...this.options, ...options };
        return new api_builder_1.APIBuilder(root || ".", this.options);
    }
    glob(...patterns) {
        if (this.globFunction) {
            return this.globWithOptions(patterns);
        }
        return this.globWithOptions(patterns, ...[{ dot: true }]);
    }
    globWithOptions(patterns, ...options) {
        const globFn = (this.globFunction || pm);
        /* c8 ignore next 5 */
        if (!globFn) {
            throw new Error('Please specify a glob function to use glob matching.');
        }
        var isMatch = this.globCache[patterns.join("\0")];
        if (!isMatch) {
            isMatch = globFn(patterns, ...options);
            this.globCache[patterns.join("\0")] = isMatch;
        }
        this.options.filters.push((path) => isMatch(path));
        return this;
    }
}
exports.Builder = Builder;
