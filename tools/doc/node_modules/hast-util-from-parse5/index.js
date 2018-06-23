'use strict';

var information = require('property-information');
var camelcase = require('camelcase');
var vfileLocation = require('vfile-location');
var h = require('hastscript');

module.exports = wrapper;

var own = {}.hasOwnProperty;

/* Handlers. */
var map = {
  '#document': root,
  '#document-fragment': root,
  '#text': text,
  '#comment': comment,
  '#documentType': doctype
};

/* Wrapper to normalise options. */
function wrapper(ast, options) {
  var settings = options || {};
  var file;

  if (settings.messages) {
    file = settings;
    settings = {};
  } else {
    file = settings.file;
  }

  return transform(ast, {
    file: file,
    toPosition: file ? vfileLocation(file).toPosition : null,
    verbose: settings.verbose,
    location: false
  });
}

/* Transform a node. */
function transform(ast, config) {
  var fn = own.call(map, ast.nodeName) ? map[ast.nodeName] : element;
  var children;
  var node;
  var position;

  if (ast.childNodes) {
    children = nodes(ast.childNodes, config);
  }

  node = fn(ast, children, config);

  if (ast.__location && config.toPosition) {
    config.location = true;
    position = location(ast.__location, ast, node, config);

    if (position) {
      node.position = position;
    }
  }

  return node;
}

/* Transform children. */
function nodes(children, config) {
  var length = children.length;
  var index = -1;
  var result = [];

  while (++index < length) {
    result[index] = transform(children[index], config);
  }

  return result;
}

/* Transform a document.
 * Stores `ast.quirksMode` in `node.data.quirksMode`. */
function root(ast, children, config) {
  var quirks = ast.mode === 'quirks' || ast.mode === 'limited-quirks';
  var node = {type: 'root', children: children};
  var position;

  node.data = {quirksMode: quirks};

  if (ast.__location) {
    if (config.toPosition) {
      config.location = true;
      position = ast.__location;
    }
  } else if (config.file && config.location) {
    position = {startOffset: 0, endOffset: String(config.file).length};
  }

  position = position && location(position, ast, node, config);

  if (position) {
    node.position = position;
  }

  return node;
}

/* Transform a doctype. */
function doctype(ast) {
  return {
    type: 'doctype',
    name: ast.name || '',
    public: ast.publicId || null,
    system: ast.systemId || null
  };
}

/* Transform a text. */
function text(ast) {
  return {type: 'text', value: ast.value};
}

/* Transform a comment. */
function comment(ast) {
  return {type: 'comment', value: ast.data};
}

/* Transform an element. */
function element(ast, children, config) {
  var props = {};
  var values = ast.attrs;
  var length = values.length;
  var index = -1;
  var attr;
  var node;
  var fragment;

  while (++index < length) {
    attr = values[index];
    props[(attr.prefix ? attr.prefix + ':' : '') + attr.name] = attr.value;
  }

  node = h(ast.tagName, props, children);

  if (ast.nodeName === 'template' && 'content' in ast) {
    fragment = ast.content;

    if (ast.__location) {
      fragment.__location = {
        startOffset: ast.__location.startTag.endOffset,
        endOffset: ast.__location.endTag.startOffset
      };
    }

    node.content = transform(ast.content, config);
  }

  return node;
}

/* Create clean positional information. */
function loc(toPosition, dirty) {
  return {
    start: toPosition(dirty.startOffset),
    end: toPosition(dirty.endOffset)
  };
}

/* Create clean positional information. */
function location(info, ast, node, config) {
  var start = info.startOffset;
  var end = info.endOffset;
  var values = info.attrs || {};
  var propPositions = {};
  var prop;
  var name;
  var reference;

  for (prop in values) {
    name = (information(prop) || {}).propertyName || camelcase(prop);
    propPositions[name] = loc(config.toPosition, values[prop]);
  }

  /* Upstream: https://github.com/inikulin/parse5/issues/109 */
  if (node.type === 'element' && !info.endTag) {
    reference = node.children[node.children.length - 1];

    /* Unclosed with children: */
    if (reference && reference.position) {
      if (reference.position.end) {
        end = reference.position.end.offset;
      } else {
        end = null;
      }
    /* Unclosed without children: */
    } else if (info.startTag) {
      end = info.startTag.endOffset;
    }
  }

  if (config.verbose && node.type === 'element') {
    node.data = {
      position: {
        opening: loc(config.toPosition, info.startTag || info),
        closing: info.endTag ? loc(config.toPosition, info.endTag) : null,
        properties: propPositions
      }
    };
  }

  start = typeof start === 'number' ? config.toPosition(start) : null;
  end = typeof end === 'number' ? config.toPosition(end) : null;

  if (!start && !end) {
    return undefined;
  }

  return {start: start, end: end};
}
