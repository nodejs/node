(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports, require('@jridgewell/set-array'), require('@jridgewell/sourcemap-codec')) :
    typeof define === 'function' && define.amd ? define(['exports', '@jridgewell/set-array', '@jridgewell/sourcemap-codec'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, factory(global.genMapping = {}, global.setArray, global.sourcemapCodec));
})(this, (function (exports, setArray, sourcemapCodec) { 'use strict';

    /**
     * A low-level API to associate a generated position with an original source position. Line and
     * column here are 0-based, unlike `addMapping`.
     */
    exports.addSegment = void 0;
    /**
     * A high-level API to associate a generated position with an original source position. Line is
     * 1-based, but column is 0-based, due to legacy behavior in `source-map` library.
     */
    exports.addMapping = void 0;
    /**
     * Adds/removes the content of the source file to the source map.
     */
    exports.setSourceContent = void 0;
    /**
     * Returns a sourcemap object (with decoded mappings) suitable for passing to a library that expects
     * a sourcemap, or to JSON.stringify.
     */
    exports.decodedMap = void 0;
    /**
     * Returns a sourcemap object (with encoded mappings) suitable for passing to a library that expects
     * a sourcemap, or to JSON.stringify.
     */
    exports.encodedMap = void 0;
    /**
     * Returns an array of high-level mapping objects for every recorded segment, which could then be
     * passed to the `source-map` library.
     */
    exports.allMappings = void 0;
    /**
     * Provides the state to generate a sourcemap.
     */
    class GenMapping {
        constructor({ file, sourceRoot } = {}) {
            this._names = new setArray.SetArray();
            this._sources = new setArray.SetArray();
            this._sourcesContent = [];
            this._mappings = [];
            this.file = file;
            this.sourceRoot = sourceRoot;
        }
    }
    (() => {
        exports.addSegment = (map, genLine, genColumn, source, sourceLine, sourceColumn, name) => {
            const { _mappings: mappings, _sources: sources, _sourcesContent: sourcesContent, _names: names, } = map;
            const line = getLine(mappings, genLine);
            if (source == null) {
                const seg = [genColumn];
                const index = getColumnIndex(line, genColumn, seg);
                return insert(line, index, seg);
            }
            const sourcesIndex = setArray.put(sources, source);
            const seg = name
                ? [genColumn, sourcesIndex, sourceLine, sourceColumn, setArray.put(names, name)]
                : [genColumn, sourcesIndex, sourceLine, sourceColumn];
            const index = getColumnIndex(line, genColumn, seg);
            if (sourcesIndex === sourcesContent.length)
                sourcesContent[sourcesIndex] = null;
            insert(line, index, seg);
        };
        exports.addMapping = (map, mapping) => {
            const { generated, source, original, name } = mapping;
            return exports.addSegment(map, generated.line - 1, generated.column, source, original == null ? undefined : original.line - 1, original === null || original === void 0 ? void 0 : original.column, name);
        };
        exports.setSourceContent = (map, source, content) => {
            const { _sources: sources, _sourcesContent: sourcesContent } = map;
            sourcesContent[setArray.put(sources, source)] = content;
        };
        exports.decodedMap = (map) => {
            const { file, sourceRoot, _mappings: mappings, _sources: sources, _sourcesContent: sourcesContent, _names: names, } = map;
            return {
                version: 3,
                file,
                names: names.array,
                sourceRoot: sourceRoot || undefined,
                sources: sources.array,
                sourcesContent,
                mappings,
            };
        };
        exports.encodedMap = (map) => {
            const decoded = exports.decodedMap(map);
            return Object.assign(Object.assign({}, decoded), { mappings: sourcemapCodec.encode(decoded.mappings) });
        };
        exports.allMappings = (map) => {
            const out = [];
            const { _mappings: mappings, _sources: sources, _names: names } = map;
            for (let i = 0; i < mappings.length; i++) {
                const line = mappings[i];
                for (let j = 0; j < line.length; j++) {
                    const seg = line[j];
                    const generated = { line: i + 1, column: seg[0] };
                    let source = undefined;
                    let original = undefined;
                    let name = undefined;
                    if (seg.length !== 1) {
                        source = sources.array[seg[1]];
                        original = { line: seg[2] + 1, column: seg[3] };
                        if (seg.length === 5)
                            name = names.array[seg[4]];
                    }
                    out.push({ generated, source, original, name });
                }
            }
            return out;
        };
    })();
    function getLine(mappings, index) {
        for (let i = mappings.length; i <= index; i++) {
            mappings[i] = [];
        }
        return mappings[index];
    }
    function getColumnIndex(line, column, seg) {
        let index = line.length;
        for (let i = index - 1; i >= 0; i--, index--) {
            const current = line[i];
            const col = current[0];
            if (col > column)
                continue;
            if (col < column)
                break;
            const cmp = compare(current, seg);
            if (cmp === 0)
                return index;
            if (cmp < 0)
                break;
        }
        return index;
    }
    function compare(a, b) {
        let cmp = compareNum(a.length, b.length);
        if (cmp !== 0)
            return cmp;
        // We've already checked genColumn
        if (a.length === 1)
            return 0;
        cmp = compareNum(a[1], b[1]);
        if (cmp !== 0)
            return cmp;
        cmp = compareNum(a[2], b[2]);
        if (cmp !== 0)
            return cmp;
        cmp = compareNum(a[3], b[3]);
        if (cmp !== 0)
            return cmp;
        if (a.length === 4)
            return 0;
        return compareNum(a[4], b[4]);
    }
    function compareNum(a, b) {
        return a - b;
    }
    function insert(array, index, value) {
        if (index === -1)
            return;
        for (let i = array.length; i > index; i--) {
            array[i] = array[i - 1];
        }
        array[index] = value;
    }

    exports.GenMapping = GenMapping;

    Object.defineProperty(exports, '__esModule', { value: true });

}));
//# sourceMappingURL=gen-mapping.umd.js.map
