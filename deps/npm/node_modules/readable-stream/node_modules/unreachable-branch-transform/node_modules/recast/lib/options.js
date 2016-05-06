var defaults = {
    // If you want to use a different branch of esprima, or any other
    // module that supports a .parse function, pass that module object to
    // recast.parse as options.parser (legacy synonym: options.esprima).
    parser: require("esprima"),

    // Number of spaces the pretty-printer should use per tab for
    // indentation. If you do not pass this option explicitly, it will be
    // (quite reliably!) inferred from the original code.
    tabWidth: 4,

    // If you really want the pretty-printer to use tabs instead of
    // spaces, make this option true.
    useTabs: false,

    // The reprinting code leaves leading whitespace untouched unless it
    // has to reindent a line, or you pass false for this option.
    reuseWhitespace: true,

    // Override this option to use a different line terminator, e.g. \r\n.
    lineTerminator: require("os").EOL,

    // Some of the pretty-printer code (such as that for printing function
    // parameter lists) makes a valiant attempt to prevent really long
    // lines. You can adjust the limit by changing this option; however,
    // there is no guarantee that line length will fit inside this limit.
    wrapColumn: 74, // Aspirational for now.

    // Pass a string as options.sourceFileName to recast.parse to tell the
    // reprinter to keep track of reused code so that it can construct a
    // source map automatically.
    sourceFileName: null,

    // Pass a string as options.sourceMapName to recast.print, and
    // (provided you passed options.sourceFileName earlier) the
    // PrintResult of recast.print will have a .map property for the
    // generated source map.
    sourceMapName: null,

    // If provided, this option will be passed along to the source map
    // generator as a root directory for relative source file paths.
    sourceRoot: null,

    // If you provide a source map that was generated from a previous call
    // to recast.print as options.inputSourceMap, the old source map will
    // be composed with the new source map.
    inputSourceMap: null,

    // If you want esprima to generate .range information (recast only
    // uses .loc internally), pass true for this option.
    range: false,

    // If you want esprima not to throw exceptions when it encounters
    // non-fatal errors, keep this option true.
    tolerant: true,

    // If you want to override the quotes used in string literals, specify
    // either "single", "double", or "auto" here ("auto" will select the one
    // which results in the shorter literal)
    // Otherwise, double quotes are used.
    quote: null,

    // If you want to print trailing commas in object literals,
    // array expressions, functions calls and function definitions pass true
    // for this option.
    trailingComma: false,
}, hasOwn = defaults.hasOwnProperty;

// Copy options and fill in default values.
exports.normalize = function(options) {
    options = options || defaults;

    function get(key) {
        return hasOwn.call(options, key)
            ? options[key]
            : defaults[key];
    }

    return {
        tabWidth: +get("tabWidth"),
        useTabs: !!get("useTabs"),
        reuseWhitespace: !!get("reuseWhitespace"),
        lineTerminator: get("lineTerminator"),
        wrapColumn: Math.max(get("wrapColumn"), 0),
        sourceFileName: get("sourceFileName"),
        sourceMapName: get("sourceMapName"),
        sourceRoot: get("sourceRoot"),
        inputSourceMap: get("inputSourceMap"),
        parser: get("esprima") || get("parser"),
        range: get("range"),
        tolerant: get("tolerant"),
        quote: get("quote"),
        trailingComma: get("trailingComma"),
    };
};
