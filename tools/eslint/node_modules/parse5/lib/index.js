'use strict';

var Parser = require('./parser'),
    Serializer = require('./serializer');

/** @namespace parse5 */

/**
 * Parses an HTML string.
 * @function parse
 * @memberof parse5
 * @instance
 * @param {string} html - Input HTML string.
 * @param {ParserOptions} [options] - Parsing options.
 * @returns {ASTNode<Document>} document
 * @example
 * var parse5 = require('parse5');
 *
 * var document = parse5.parse('<!DOCTYPE html><html><head></head><body>Hi there!</body></html>');
 */
exports.parse = function parse(html, options) {
    var parser = new Parser(options);

    return parser.parse(html);
};

/**
 * Parses an HTML fragment.
 * @function parseFragment
 * @memberof parse5
 * @instance
 * @param {ASTNode} [fragmentContext] - Parsing context element. If specified, given fragment
 * will be parsed as if it was set to the context element's `innerHTML` property.
 * @param {string} html - Input HTML fragment string.
 * @param {ParserOptions} [options] - Parsing options.
 * @returns {ASTNode<DocumentFragment>} documentFragment
 * @example
 * var parse5 = require('parse5');
 *
 * var documentFragment = parse5.parseFragment('<table></table>');
 *
 * // Parses the html fragment in the context of the parsed <table> element.
 * var trFragment = parser.parseFragment(documentFragment.childNodes[0], '<tr><td>Shake it, baby</td></tr>');
 */
exports.parseFragment = function parseFragment(fragmentContext, html, options) {
    if (typeof fragmentContext === 'string') {
        options = html;
        html = fragmentContext;
        fragmentContext = null;
    }

    var parser = new Parser(options);

    return parser.parseFragment(html, fragmentContext);
};

/**
 * Serializes an AST node to an HTML string.
 * @function serialize
 * @memberof parse5
 * @instance
 * @param {ASTNode} node - Node to serialize.
 * @param {SerializerOptions} [options] - Serialization options.
 * @returns {String} html
 * @example
 * var parse5 = require('parse5');
 *
 * var document = parse5.parse('<!DOCTYPE html><html><head></head><body>Hi there!</body></html>');
 *
 * // Serializes a document.
 * var html = parse5.serialize(document);
 *
 * // Serializes the <body> element content.
 * var bodyInnerHtml = parse5.serialize(document.childNodes[0].childNodes[1]);
 */
exports.serialize = function (node, options) {
    var serializer = new Serializer(node, options);

    return serializer.serialize();
};

/**
 * Provides built-in tree adapters that can be used for parsing and serialization.
 * @var treeAdapters
 * @memberof parse5
 * @instance
 * @property {TreeAdapter} default - Default tree format for parse5.
 * @property {TreeAdapter} htmlparser2 - Quite popular [htmlparser2](https://github.com/fb55/htmlparser2) tree format
 * (e.g. used by [cheerio](https://github.com/MatthewMueller/cheerio) and [jsdom](https://github.com/tmpvar/jsdom)).
 * @example
 * var parse5 = require('parse5');
 *
 * // Uses the default tree adapter for parsing.
 * var document = parse5.parse('<div></div>', { treeAdapter: parse5.treeAdapters.default });
 *
 * // Uses the htmlparser2 tree adapter with the SerializerStream.
 * var serializer = new parse5.SerializerStream(node, { treeAdapter: parse5.treeAdapters.htmlparser2 });
 */
exports.treeAdapters = {
    default: require('./tree_adapters/default'),
    htmlparser2: require('./tree_adapters/htmlparser2')
};


// Streaming
exports.ParserStream = require('./parser/stream');
exports.SerializerStream = require('./serializer/stream');
exports.SAXParser = require('./sax');
