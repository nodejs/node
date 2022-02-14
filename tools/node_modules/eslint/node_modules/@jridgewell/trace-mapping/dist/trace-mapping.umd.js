(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports, require('@jridgewell/sourcemap-codec'), require('@jridgewell/resolve-uri')) :
    typeof define === 'function' && define.amd ? define(['exports', '@jridgewell/sourcemap-codec', '@jridgewell/resolve-uri'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, factory(global.traceMapping = {}, global.sourcemapCodec, global.resolveURI));
})(this, (function (exports, sourcemapCodec, resolveUri) { 'use strict';

    function _interopDefaultLegacy (e) { return e && typeof e === 'object' && 'default' in e ? e : { 'default': e }; }

    var resolveUri__default = /*#__PURE__*/_interopDefaultLegacy(resolveUri);

    function resolve(input, base) {
        // The base is always treated as a directory, if it's not empty.
        // https://github.com/mozilla/source-map/blob/8cb3ee57/lib/util.js#L327
        // https://github.com/chromium/chromium/blob/da4adbb3/third_party/blink/renderer/devtools/front_end/sdk/SourceMap.js#L400-L401
        if (base && !base.endsWith('/'))
            base += '/';
        return resolveUri__default["default"](input, base);
    }

    /**
     * Removes everything after the last "/", but leaves the slash.
     */
    function stripFilename(path) {
        if (!path)
            return '';
        const index = path.lastIndexOf('/');
        return path.slice(0, index + 1);
    }

    function maybeSort(mappings, owned) {
        const unsortedIndex = nextUnsortedSegmentLine(mappings, 0);
        if (unsortedIndex === mappings.length)
            return mappings;
        // If we own the array (meaning we parsed it from JSON), then we're free to directly mutate it. If
        // not, we do not want to modify the consumer's input array.
        if (!owned)
            mappings = mappings.slice();
        for (let i = unsortedIndex; i < mappings.length; i = nextUnsortedSegmentLine(mappings, i + 1)) {
            mappings[i] = sortSegments(mappings[i], owned);
        }
        return mappings;
    }
    function nextUnsortedSegmentLine(mappings, start) {
        for (let i = start; i < mappings.length; i++) {
            if (!isSorted(mappings[i]))
                return i;
        }
        return mappings.length;
    }
    function isSorted(line) {
        for (let j = 1; j < line.length; j++) {
            if (line[j][0] < line[j - 1][0]) {
                return false;
            }
        }
        return true;
    }
    function sortSegments(line, owned) {
        if (!owned)
            line = line.slice();
        return line.sort(sortComparator);
    }
    function sortComparator(a, b) {
        return a[0] - b[0];
    }

    /**
     * A binary search implementation that returns the index if a match is found.
     * If no match is found, then the left-index (the index associated with the item that comes just
     * before the desired index) is returned. To maintain proper sort order, a splice would happen at
     * the next index:
     *
     * ```js
     * const array = [1, 3];
     * const needle = 2;
     * const index = binarySearch(array, needle, (item, needle) => item - needle);
     *
     * assert.equal(index, 0);
     * array.splice(index + 1, 0, needle);
     * assert.deepEqual(array, [1, 2, 3]);
     * ```
     */
    function binarySearch(haystack, needle, low, high) {
        while (low <= high) {
            const mid = low + ((high - low) >> 1);
            const cmp = haystack[mid][0] - needle;
            if (cmp === 0) {
                return mid;
            }
            if (cmp < 0) {
                low = mid + 1;
            }
            else {
                high = mid - 1;
            }
        }
        return low - 1;
    }
    function memoizedState() {
        return {
            lastKey: -1,
            lastNeedle: -1,
            lastIndex: -1,
        };
    }
    /**
     * This overly complicated beast is just to record the last tested line/column and the resulting
     * index, allowing us to skip a few tests if mappings are monotonically increasing.
     */
    function memoizedBinarySearch(haystack, needle, state, key) {
        const { lastKey, lastNeedle, lastIndex } = state;
        let low = 0;
        let high = haystack.length - 1;
        if (key === lastKey) {
            if (needle === lastNeedle) {
                return lastIndex;
            }
            if (needle >= lastNeedle) {
                // lastIndex may be -1 if the previous needle was not found.
                low = Math.max(lastIndex, 0);
            }
            else {
                high = lastIndex;
            }
        }
        state.lastKey = key;
        state.lastNeedle = needle;
        return (state.lastIndex = binarySearch(haystack, needle, low, high));
    }

    const INVALID_MAPPING = Object.freeze({
        source: null,
        line: null,
        column: null,
        name: null,
    });
    /**
     * Returns the encoded (VLQ string) form of the SourceMap's mappings field.
     */
    exports.encodedMappings = void 0;
    /**
     * Returns the decoded (array of lines of segments) form of the SourceMap's mappings field.
     */
    exports.decodedMappings = void 0;
    /**
     * A low-level API to find the segment associated with a generated line/column (think, from a
     * stack trace). Line and column here are 0-based, unlike `originalPositionFor`.
     */
    exports.traceSegment = void 0;
    /**
     * A higher-level API to find the source/line/column associated with a generated line/column
     * (think, from a stack trace). Line is 1-based, but column is 0-based, due to legacy behavior in
     * `source-map` library.
     */
    exports.originalPositionFor = void 0;
    /**
     * Iterates each mapping in generated position order.
     */
    exports.eachMapping = void 0;
    /**
     * A helper that skips sorting of the input map's mappings array, which can be expensive for larger
     * maps.
     */
    exports.presortedDecodedMap = void 0;
    class TraceMap {
        constructor(map, mapUrl) {
            this._binarySearchMemo = memoizedState();
            const isString = typeof map === 'string';
            const parsed = isString ? JSON.parse(map) : map;
            const { version, file, names, sourceRoot, sources, sourcesContent } = parsed;
            this.version = version;
            this.file = file;
            this.names = names;
            this.sourceRoot = sourceRoot;
            this.sources = sources;
            this.sourcesContent = sourcesContent;
            if (sourceRoot || mapUrl) {
                const from = resolve(sourceRoot || '', stripFilename(mapUrl));
                this.resolvedSources = sources.map((s) => resolve(s || '', from));
            }
            else {
                this.resolvedSources = sources.map((s) => s || '');
            }
            const { mappings } = parsed;
            if (typeof mappings === 'string') {
                this._encoded = mappings;
                this._decoded = sourcemapCodec.decode(mappings);
            }
            else {
                this._encoded = undefined;
                this._decoded = maybeSort(mappings, isString);
            }
        }
    }
    (() => {
        exports.encodedMappings = (map) => {
            var _a;
            return ((_a = map._encoded) !== null && _a !== void 0 ? _a : (map._encoded = sourcemapCodec.encode(map._decoded)));
        };
        exports.decodedMappings = (map) => {
            return map._decoded;
        };
        exports.traceSegment = (map, line, column) => {
            const decoded = map._decoded;
            // It's common for parent source maps to have pointers to lines that have no
            // mapping (like a "//# sourceMappingURL=") at the end of the child file.
            if (line >= decoded.length)
                return null;
            const segments = decoded[line];
            const index = memoizedBinarySearch(segments, column, map._binarySearchMemo, line);
            // we come before any mapped segment
            if (index < 0)
                return null;
            return segments[index];
        };
        exports.originalPositionFor = (map, { line, column }) => {
            if (line < 1)
                throw new Error('`line` must be greater than 0 (lines start at line 1)');
            if (column < 0) {
                throw new Error('`column` must be greater than or equal to 0 (columns start at column 0)');
            }
            const segment = exports.traceSegment(map, line - 1, column);
            if (segment == null)
                return INVALID_MAPPING;
            if (segment.length == 1)
                return INVALID_MAPPING;
            const { names, resolvedSources } = map;
            return {
                source: resolvedSources[segment[1]],
                line: segment[2] + 1,
                column: segment[3],
                name: segment.length === 5 ? names[segment[4]] : null,
            };
        };
        exports.eachMapping = (map, cb) => {
            const decoded = map._decoded;
            const { names, resolvedSources } = map;
            for (let i = 0; i < decoded.length; i++) {
                const line = decoded[i];
                for (let j = 0; j < line.length; j++) {
                    const seg = line[j];
                    const generatedLine = i + 1;
                    const generatedColumn = seg[0];
                    let source = null;
                    let originalLine = null;
                    let originalColumn = null;
                    let name = null;
                    if (seg.length !== 1) {
                        source = resolvedSources[seg[1]];
                        originalLine = seg[2] + 1;
                        originalColumn = seg[3];
                    }
                    if (seg.length === 5)
                        name = names[seg[4]];
                    cb({
                        generatedLine,
                        generatedColumn,
                        source,
                        originalLine,
                        originalColumn,
                        name,
                    });
                }
            }
        };
        exports.presortedDecodedMap = (map, mapUrl) => {
            const clone = Object.assign({}, map);
            clone.mappings = [];
            const tracer = new TraceMap(clone, mapUrl);
            tracer._decoded = map.mappings;
            return tracer;
        };
    })();

    exports.TraceMap = TraceMap;

    Object.defineProperty(exports, '__esModule', { value: true });

}));
//# sourceMappingURL=trace-mapping.umd.js.map
