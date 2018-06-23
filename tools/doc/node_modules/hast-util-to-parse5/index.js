'use strict';

var xtend = require('xtend');
var toH = require('hast-to-hyperscript');
var NS = require('web-namespaces');
var zwitch = require('zwitch');
var mapz = require('mapz');

module.exports = transform;

var own = {}.hasOwnProperty;
var one = zwitch('type');
var all = mapz(one, {key: 'children', indices: false});

var customProps = ['__location', 'childNodes', 'content', 'parentNode', 'namespaceURI'];

one.handlers.root = root;
one.handlers.element = element;
one.handlers.text = text;
one.handlers.comment = comment;
one.handlers.doctype = doctype;

/* Map of tag-names starting new namespaces. */
var namespaces = {
  math: NS.mathml,
  svg: NS.svg
};

/* Map of attributes with namespaces. */
var attributeSpaces = {
  'xlink:actuate': {prefix: 'xlink', name: 'actuate', namespace: NS.xlink},
  'xlink:arcrole': {prefix: 'xlink', name: 'arcrole', namespace: NS.xlink},
  'xlink:href': {prefix: 'xlink', name: 'href', namespace: NS.xlink},
  'xlink:role': {prefix: 'xlink', name: 'role', namespace: NS.xlink},
  'xlink:show': {prefix: 'xlink', name: 'show', namespace: NS.xlink},
  'xlink:title': {prefix: 'xlink', name: 'title', namespace: NS.xlink},
  'xlink:type': {prefix: 'xlink', name: 'type', namespace: NS.xlink},
  'xml:base': {prefix: 'xml', name: 'base', namespace: NS.xml},
  'xml:lang': {prefix: 'xml', name: 'lang', namespace: NS.xml},
  'xml:space': {prefix: 'xml', name: 'space', namespace: NS.xml},
  xmlns: {prefix: '', name: 'xmlns', namespace: NS.xmlns},
  'xmlns:xlink': {prefix: 'xmlns', name: 'xlink', namespace: NS.xmlns}
};

/* Transform a tree from HAST to Parse5’s AST. */
function transform(tree) {
  return patch(one(tree), null, NS.html);
}

function root(node) {
  var data = node.data || {};
  var qs = own.call(data, 'quirksMode') ? Boolean(data.quirksMode) : false;

  return {
    nodeName: '#document',
    mode: qs ? 'quirks' : 'no-quirks',
    childNodes: all(node)
  };
}

function element(node) {
  var shallow = xtend(node);

  shallow.children = [];

  return toH(function (name, attrs) {
    var values = [];
    var content;
    var value;
    var key;

    for (key in attrs) {
      value = {name: key, value: attrs[key]};

      if (own.call(attributeSpaces, key)) {
        value = xtend(value, attributeSpaces[key]);
      }

      values.push(value);
    }

    if (name === 'template') {
      content = transform(shallow.content);
      delete content.mode;
      content.nodeName = '#document-fragment';
    }

    return wrap(node, {
      nodeName: node.tagName,
      tagName: node.tagName,
      attrs: values,
      childNodes: node.children ? all(node) : []
    }, content);
  }, shallow);
}

function doctype(node) {
  return wrap(node, {
    nodeName: '#documentType',
    name: node.name,
    publicId: node.public || null,
    systemId: node.system || null
  });
}

function text(node) {
  return wrap(node, {
    nodeName: '#text',
    value: node.value
  });
}

function comment(node) {
  return wrap(node, {
    nodeName: '#comment',
    data: node.value
  });
}

/* Patch position. */
function wrap(node, ast, content) {
  if (node.position && node.position.start && node.position.end) {
    ast.__location = {
      line: node.position.start.line,
      col: node.position.start.column,
      startOffset: node.position.start.offset,
      endOffset: node.position.end.offset
    };
  }

  if (content) {
    ast.content = content;
  }

  return ast;
}

/* Patch a tree recursively, by adding namespaces
 * and parent references where needed. */
function patch(node, parent, ns) {
  var location = node.__location;
  var children = node.childNodes;
  var name = node.tagName;
  var replacement = {};
  var length;
  var index;
  var key;

  for (key in node) {
    if (customProps.indexOf(key) === -1) {
      replacement[key] = node[key];
    }
  }

  if (own.call(namespaces, name)) {
    ns = namespaces[name];
  }

  if (own.call(replacement, 'tagName')) {
    replacement.namespaceURI = ns;
  }

  if (children) {
    replacement.childNodes = children;
    length = children.length;
    index = -1;

    while (++index < length) {
      children[index] = patch(children[index], replacement, ns);
    }
  }

  if (name === 'template') {
    replacement.content = patch(node.content, null, ns);
  }

  if (parent) {
    replacement.parentNode = parent;
  }

  if (location) {
    replacement.__location = location;
  }

  return replacement;
}
