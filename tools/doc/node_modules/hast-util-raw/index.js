'use strict';

var Parser = require('parse5/lib/parser');
var pos = require('unist-util-position');
var fromParse5 = require('hast-util-from-parse5');
var toParse5 = require('hast-util-to-parse5');
var voids = require('html-void-elements');
var ns = require('web-namespaces');
var zwitch = require('zwitch');

module.exports = wrap;

var IN_TEMPLATE_MODE = 'IN_TEMPLATE_MODE';
var CHARACTER_TOKEN = 'CHARACTER_TOKEN';
var START_TAG_TOKEN = 'START_TAG_TOKEN';
var END_TAG_TOKEN = 'END_TAG_TOKEN';
var HIBERNATION_TOKEN = 'HIBERNATION_TOKEN';
var COMMENT_TOKEN = 'COMMENT_TOKEN';
var DOCTYPE_TOKEN = 'DOCTYPE_TOKEN';
var DOCUMENT = 'document';
var FRAGMENT = 'fragment';

function wrap(tree, file) {
  var parser = new Parser({locationInfo: true});
  var one = zwitch('type');
  var mode = inferMode(tree);
  var preprocessor;
  var result;

  one.handlers.root = root;
  one.handlers.element = element;
  one.handlers.text = text;
  one.handlers.comment = comment;
  one.handlers.doctype = doctype;
  one.handlers.raw = raw;
  one.unknown = unknown;

  result = fromParse5(mode === FRAGMENT ? fragment() : document(), file);

  /* Unpack if possible and when not given a `root`. */
  if (tree.type !== 'root' && result.children.length === 1) {
    return result.children[0];
  }

  return result;

  function fragment() {
    var context;
    var mock;
    var doc;

    context = {
      nodeName: 'template',
      tagName: 'template',
      attrs: [],
      namespaceURI: ns.html,
      childNodes: []
    };

    mock = {
      nodeName: 'documentmock',
      tagName: 'documentmock',
      attrs: [],
      namespaceURI: ns.html,
      childNodes: []
    };

    doc = {
      nodeName: '#document-fragment',
      childNodes: []
    };

    parser._bootstrap(mock, context);
    parser._pushTmplInsertionMode(IN_TEMPLATE_MODE);
    parser._initTokenizerForFragmentParsing();
    parser._insertFakeRootElement();
    parser._resetInsertionMode();
    parser._findFormInFragmentContext();

    preprocessor = parser.tokenizer.preprocessor;

    one(tree);

    parser._adoptNodes(mock.childNodes[0], doc);

    return doc;
  }

  function document() {
    var doc = parser.treeAdapter.createDocument();

    parser._bootstrap(doc, null);

    one(tree);

    return doc;
  }

  function all(nodes) {
    var length = 0;
    var index = -1;

    /* istanbul ignore else - invalid nodes, see wooorm/rehype-raw#7. */
    if (nodes) {
      length = nodes.length;
    }

    while (++index < length) {
      one(nodes[index]);
    }
  }

  function root(node) {
    all(node.children);
  }

  function element(node) {
    var empty = voids.indexOf(node.tagName) !== -1;

    parser._processToken(startTag(node), ns.html);

    all(node.children);

    if (!empty) {
      parser._processToken(endTag(node));
    }
  }

  function text(node) {
    var start = pos.start(node);
    parser._processToken({
      type: CHARACTER_TOKEN,
      chars: node.value,
      location: {
        line: start.line,
        col: start.column,
        startOffset: start.offset,
        endOffset: pos.end(node).offset
      }
    });
  }

  function doctype(node) {
    var p5 = toParse5(node);
    parser._processToken({
      type: DOCTYPE_TOKEN,
      name: p5.name,
      forceQuirks: false,
      publicId: p5.publicId,
      systemId: p5.systemId
    });
  }

  function comment(node) {
    var start = pos.start(node);
    parser._processToken({
      type: COMMENT_TOKEN,
      data: node.value,
      location: {
        line: start.line,
        col: start.column,
        startOffset: start.offset,
        endOffset: pos.end(node).offset
      }
    });
  }

  function raw(node) {
    var start = pos.start(node).offset;

    preprocessor.html = null;
    preprocessor.lastCharPos = -1;
    preprocessor.pos = -1;

    if (start !== null) {
      preprocessor.__locTracker.droppedBufferSize = start;
    }

    parser.tokenizer.write(node.value);

    run(parser);
  }
}

function run(p) {
  var tokenizer = p.tokenizer;
  var token;

  while (!p.stopped) {
    p._setupTokenizerCDATAMode();

    token = tokenizer.getNextToken();

    if (token.type === HIBERNATION_TOKEN) {
      token = tokenizer.currentCharacterToken || tokenizer.currentToken;

      if (token) {
        p._processInputToken(token);
      }

      tokenizer.currentToken = null;
      tokenizer.currentCharacterToken = null;

      break;
    }

    p._processInputToken(token);
  }
}

function startTag(node) {
  var start = pos.start(node);
  var end = pos.end(node);

  return {
    type: START_TAG_TOKEN,
    tagName: node.tagName,
    selfClosing: false,
    attrs: attributes(node),
    location: {
      line: start.line,
      col: start.column,
      startOffset: start.offset,
      endOffset: end.offset,
      attrs: {},
      startTag: {
        line: start.line,
        col: start.column,
        startOffset: start.offset,
        endOffset: end.offset
      }
    }
  };
}

function attributes(node) {
  return toParse5({
    type: 'element',
    properties: node.properties
  }).attrs;
}

function endTag(node) {
  var end = pos.end(node);

  return {
    type: END_TAG_TOKEN,
    tagName: node.tagName,
    attrs: [],
    location: {
      line: end.line,
      col: end.column,
      startOffset: end.offset,
      endOffset: end.offset
    }
  };
}

function unknown(node) {
  throw new Error('Cannot compile `' + node.type + '` node');
}

function inferMode(node) {
  var head = node.type === 'root' ? node.children[0] : node;

  if (head && (head.type === 'doctype' || head.tagName === 'html')) {
    return DOCUMENT;
  }

  return FRAGMENT;
}
