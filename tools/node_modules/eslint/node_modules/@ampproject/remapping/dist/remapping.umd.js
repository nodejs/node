(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? module.exports = factory(require('@jridgewell/trace-mapping')) :
    typeof define === 'function' && define.amd ? define(['@jridgewell/trace-mapping'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, global.remapping = factory(global.traceMapping));
})(this, (function (traceMapping) { 'use strict';

    /**
     * A "leaf" node in the sourcemap tree, representing an original, unmodified
     * source file. Recursive segment tracing ends at the `OriginalSource`.
     */
    class OriginalSource {
        constructor(source, content) {
            this.source = source;
            this.content = content;
        }
        /**
         * Tracing a `SourceMapSegment` ends when we get to an `OriginalSource`,
         * meaning this line/column location originated from this source file.
         */
        originalPositionFor(line, column, name) {
            return { column, line, name, source: this.source, content: this.content };
        }
    }

    /**
     * Puts `key` into the backing array, if it is not already present. Returns
     * the index of the `key` in the backing array.
     */
    let put;
    /**
     * FastStringArray acts like a `Set` (allowing only one occurrence of a string
     * `key`), but provides the index of the `key` in the backing array.
     *
     * This is designed to allow synchronizing a second array with the contents of
     * the backing array, like how `sourcesContent[i]` is the source content
     * associated with `source[i]`, and there are never duplicates.
     */
    class FastStringArray {
        constructor() {
            this.indexes = Object.create(null);
            this.array = [];
        }
    }
    (() => {
        put = (strarr, key) => {
            const { array, indexes } = strarr;
            // The key may or may not be present. If it is present, it's a number.
            let index = indexes[key];
            // If it's not yet present, we need to insert it and track the index in the
            // indexes.
            if (index === undefined) {
                index = indexes[key] = array.length;
                array.push(key);
            }
            return index;
        };
    })();

    const INVALID_MAPPING = undefined;
    const SOURCELESS_MAPPING = null;
    /**
     * traceMappings is only called on the root level SourceMapTree, and begins the process of
     * resolving each mapping in terms of the original source files.
     */
    let traceMappings;
    /**
     * SourceMapTree represents a single sourcemap, with the ability to trace
     * mappings into its child nodes (which may themselves be SourceMapTrees).
     */
    class SourceMapTree {
        constructor(map, sources) {
            this.map = map;
            this.sources = sources;
        }
        /**
         * originalPositionFor is only called on children SourceMapTrees. It recurses down
         * into its own child SourceMapTrees, until we find the original source map.
         */
        originalPositionFor(line, column, name) {
            const segment = traceMapping.traceSegment(this.map, line, column);
            // If we couldn't find a segment, then this doesn't exist in the sourcemap.
            if (segment == null)
                return INVALID_MAPPING;
            // 1-length segments only move the current generated column, there's no source information
            // to gather from it.
            if (segment.length === 1)
                return SOURCELESS_MAPPING;
            const source = this.sources[segment[1]];
            return source.originalPositionFor(segment[2], segment[3], segment.length === 5 ? this.map.names[segment[4]] : name);
        }
    }
    (() => {
        traceMappings = (tree) => {
            const mappings = [];
            const names = new FastStringArray();
            const sources = new FastStringArray();
            const sourcesContent = [];
            const { sources: rootSources, map } = tree;
            const rootNames = map.names;
            const rootMappings = traceMapping.decodedMappings(map);
            let lastLineWithSegment = -1;
            for (let i = 0; i < rootMappings.length; i++) {
                const segments = rootMappings[i];
                const tracedSegments = [];
                let lastSourcesIndex = -1;
                let lastSourceLine = -1;
                let lastSourceColumn = -1;
                for (let j = 0; j < segments.length; j++) {
                    const segment = segments[j];
                    let traced = SOURCELESS_MAPPING;
                    // 1-length segments only move the current generated column, there's no source information
                    // to gather from it.
                    if (segment.length !== 1) {
                        const source = rootSources[segment[1]];
                        traced = source.originalPositionFor(segment[2], segment[3], segment.length === 5 ? rootNames[segment[4]] : '');
                        // If the trace is invalid, then the trace ran into a sourcemap that doesn't contain a
                        // respective segment into an original source.
                        if (traced === INVALID_MAPPING)
                            continue;
                    }
                    const genCol = segment[0];
                    if (traced === SOURCELESS_MAPPING) {
                        if (lastSourcesIndex === -1) {
                            // This is a consecutive source-less segment, which doesn't carry any new information.
                            continue;
                        }
                        lastSourcesIndex = lastSourceLine = lastSourceColumn = -1;
                        tracedSegments.push([genCol]);
                        continue;
                    }
                    // So we traced a segment down into its original source file. Now push a
                    // new segment pointing to this location.
                    const { column, line, name, content, source } = traced;
                    // Store the source location, and ensure we keep sourcesContent up to
                    // date with the sources array.
                    const sourcesIndex = put(sources, source);
                    sourcesContent[sourcesIndex] = content;
                    if (lastSourcesIndex === sourcesIndex &&
                        lastSourceLine === line &&
                        lastSourceColumn === column) {
                        // This is a duplicate mapping pointing at the exact same starting point in the source
                        // file. It doesn't carry any new information, and only bloats the sourcemap.
                        continue;
                    }
                    lastLineWithSegment = i;
                    lastSourcesIndex = sourcesIndex;
                    lastSourceLine = line;
                    lastSourceColumn = column;
                    // This looks like unnecessary duplication, but it noticeably increases performance. If we
                    // were to push the nameIndex onto length-4 array, v8 would internally allocate 22 slots!
                    // That's 68 wasted bytes! Array literals have the same capacity as their length, saving
                    // memory.
                    tracedSegments.push(name
                        ? [genCol, sourcesIndex, line, column, put(names, name)]
                        : [genCol, sourcesIndex, line, column]);
                }
                mappings.push(tracedSegments);
            }
            if (mappings.length > lastLineWithSegment + 1) {
                mappings.length = lastLineWithSegment + 1;
            }
            return traceMapping.presortedDecodedMap(Object.assign({}, tree.map, {
                mappings,
                // TODO: Make all sources relative to the sourceRoot.
                sourceRoot: undefined,
                names: names.array,
                sources: sources.array,
                sourcesContent,
            }));
        };
    })();

    function asArray(value) {
        if (Array.isArray(value))
            return value;
        return [value];
    }
    /**
     * Recursively builds a tree structure out of sourcemap files, with each node
     * being either an `OriginalSource` "leaf" or a `SourceMapTree` composed of
     * `OriginalSource`s and `SourceMapTree`s.
     *
     * Every sourcemap is composed of a collection of source files and mappings
     * into locations of those source files. When we generate a `SourceMapTree` for
     * the sourcemap, we attempt to load each source file's own sourcemap. If it
     * does not have an associated sourcemap, it is considered an original,
     * unmodified source file.
     */
    function buildSourceMapTree(input, loader) {
        const maps = asArray(input).map((m) => new traceMapping.TraceMap(m, ''));
        const map = maps.pop();
        for (let i = 0; i < maps.length; i++) {
            if (maps[i].sources.length > 1) {
                throw new Error(`Transformation map ${i} must have exactly one source file.\n` +
                    'Did you specify these with the most recent transformation maps first?');
            }
        }
        let tree = build(map, loader, '', 0);
        for (let i = maps.length - 1; i >= 0; i--) {
            tree = new SourceMapTree(maps[i], [tree]);
        }
        return tree;
    }
    function build(map, loader, importer, importerDepth) {
        const { resolvedSources, sourcesContent } = map;
        const depth = importerDepth + 1;
        const children = resolvedSources.map((sourceFile, i) => {
            // The loading context gives the loader more information about why this file is being loaded
            // (eg, from which importer). It also allows the loader to override the location of the loaded
            // sourcemap/original source, or to override the content in the sourcesContent field if it's
            // an unmodified source file.
            const ctx = {
                importer,
                depth,
                source: sourceFile || '',
                content: undefined,
            };
            // Use the provided loader callback to retrieve the file's sourcemap.
            // TODO: We should eventually support async loading of sourcemap files.
            const sourceMap = loader(ctx.source, ctx);
            const { source, content } = ctx;
            // If there is no sourcemap, then it is an unmodified source file.
            if (!sourceMap) {
                // The contents of this unmodified source file can be overridden via the loader context,
                // allowing it to be explicitly null or a string. If it remains undefined, we fall back to
                // the importing sourcemap's `sourcesContent` field.
                const sourceContent = content !== undefined ? content : sourcesContent ? sourcesContent[i] : null;
                return new OriginalSource(source, sourceContent);
            }
            // Else, it's a real sourcemap, and we need to recurse into it to load its
            // source files.
            return build(new traceMapping.TraceMap(sourceMap, source), loader, source, depth);
        });
        return new SourceMapTree(map, children);
    }

    /**
     * A SourceMap v3 compatible sourcemap, which only includes fields that were
     * provided to it.
     */
    class SourceMap {
        constructor(map, options) {
            this.version = 3; // SourceMap spec says this should be first.
            this.file = map.file;
            this.mappings = options.decodedMappings ? traceMapping.decodedMappings(map) : traceMapping.encodedMappings(map);
            this.names = map.names;
            this.sourceRoot = map.sourceRoot;
            this.sources = map.sources;
            if (!options.excludeContent && 'sourcesContent' in map) {
                this.sourcesContent = map.sourcesContent;
            }
        }
        toString() {
            return JSON.stringify(this);
        }
    }

    /**
     * Traces through all the mappings in the root sourcemap, through the sources
     * (and their sourcemaps), all the way back to the original source location.
     *
     * `loader` will be called every time we encounter a source file. If it returns
     * a sourcemap, we will recurse into that sourcemap to continue the trace. If
     * it returns a falsey value, that source file is treated as an original,
     * unmodified source file.
     *
     * Pass `excludeContent` to exclude any self-containing source file content
     * from the output sourcemap.
     *
     * Pass `decodedMappings` to receive a SourceMap with decoded (instead of
     * VLQ encoded) mappings.
     */
    function remapping(input, loader, options) {
        const opts = typeof options === 'object' ? options : { excludeContent: !!options, decodedMappings: false };
        const tree = buildSourceMapTree(input, loader);
        return new SourceMap(traceMappings(tree), opts);
    }

    return remapping;

}));
//# sourceMappingURL=remapping.umd.js.map
