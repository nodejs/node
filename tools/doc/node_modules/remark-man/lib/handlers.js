'use strict';

/* Dependencies. */
var toString = require('mdast-util-to-string');
var escape = require('./escape');
var quote = require('./quote');
var macro = require('./macro');

/* Expose. */
exports.inlineCode = inlineCode;
exports.strong = bold;
exports.emphasis = italic;
exports.delete = italic;
exports.break = hardBreak;
exports.link = link;
exports.image = link;
exports.heading = heading;
exports.root = root;
exports.paragraph = paragraph;
exports.thematicBreak = rule;
exports.blockquote = blockquote;
exports.code = code;
exports.list = list;
exports.listItem = listItem;
exports.text = text;
exports.escape = text;
exports.linkReference = reference;
exports.imageReference = reference;
exports.table = table;
exports.definition = Function.prototype;

/* Constants. */
var MAILTO = 'mailto:';
var PARAGRAPH = macro('P', '\n');

/* Wrap a value in a text decoration. */
function textDecoration(enter, value, exit) {
  return '\\f' + enter + (value ? value : '') + '\\f' + exit;
}

/* Wrap a node in an inline roff command. */
function inline(decoration, node, compiler) {
  var exit = compiler.exitMarker || 'R';
  var value = node;

  compiler.exitMarker = decoration;

  if (node && node.type) {
    value = compiler.all(node).join('');
  }

  compiler.exitMarker = exit;

  return textDecoration(decoration, value, exit);
}

/* Compile a value as bold. */
function bold(node) {
  return inline('B', node, this);
}

/* Compile a value as italic. */
function italic(node) {
  return inline('I', node, this);
}

/* Compile inline code. */
function inlineCode(node) {
  return inline('B', escape(node.value), this);
}

/* Compile a break. */
function hardBreak() {
  return '\n' + macro('br') + '\n';
}

/* Compile a horizontal rule. */
function rule() {
  return '\n\\(em\\(em\\(em';
}

/* Compile a paragraph. */
function paragraph(node) {
  return macro('P', '\n' + this.all(node).join(''));
}

/* Compile a heading. */
function heading(node) {
  var depth = node.depth + (this.increaseDepth ? 1 : 0);
  var name = depth === 2 ? 'SH' : 'SS';
  var value;

  if (node === this.mainHeading) {
    return;
  }

  value = this.all(node).join('');

  /* Convert top-level section names to ALL-CAPS. */
  if (name === 'SH') {
    value = value.toUpperCase();
  }

  return macro(name, quote(value));
}

/* Compile a link. */
function link(node, href) {
  var self = this;
  var value = 'children' in node ? self.all(node).join('') : node.alt;
  var url = escape(typeof href === 'string' ? href : node.url || '');
  var head;

  if (url && url.slice(0, MAILTO.length) === MAILTO) {
    url = url.slice(MAILTO.length);
  }

  head = url.charAt(0) === '#' && self.headings[url.slice(1)];

  if (head) {
    url = '(' + escape(toString(head)) + ')';
  } else {
    if (value && escape(url) === value) {
      value = '';
    }

    if (url) {
      url = '\\(la' + escape(url) + '\\(ra';
    }
  }

  if (value) {
    value = bold.call(self, value);

    if (url) {
      value += ' ';
    }
  }

  return value + (url ? italic.call(self, url) : '');
}

/* Compile a reference. */
function reference(node) {
  var url = this.links[node.identifier.toUpperCase()];

  return link.call(this, node, url);
}

/* Compile code. */
function code(node) {
  return '.P\n' +
    '.RS 2\n' +
    '.nf\n' +
    escape(node.value) + '\n' +
    '.fi\n' +
    '.RE';
}

/* Compile a block quote. */
function blockquote(node) {
  var self = this;
  var value;

  self.level++;

  value = self.all(node).join('\n');

  self.level--;

  value = '.RS ' + (self.level ? 4 : 0) + '\n' + value + '\n.RE 0\n';

  return value;
}

/* Compile text. */
function text(node) {
  return escape(node.value.replace(/[\n ]+/g, ' '));
}

/* Compile a list. */
function list(node) {
  var self = this;
  var start = node.start;
  var children = node.children;
  var length = children.length;
  var index = -1;
  var values = [];
  var bullet;

  self.level++;

  while (++index < length) {
    bullet = start ? start + index + '.' : '\\(bu';
    values.push(listItem.call(self, children[index], bullet, index));
  }

  self.level--;

  return '.RS ' + (this.level ? 4 : 0) + '\n' +
    values.join('\n') + '\n' +
    '.RE 0\n';
}

/* Compile a list-item. */
function listItem(node, bullet) {
  var result = this.all(node).join('\n').slice(PARAGRAPH.length);
  return '.IP ' + bullet + ' 4\n' + result;
}

/* Compile a table. */
function table(node) {
  var self = this;
  var rows = node.children;
  var index = rows.length;
  var align = node.align;
  var alignLength = align.length;
  var pos;
  var result = [];
  var row;
  var out;
  var alignHeading = [];
  var alignRow = [];

  while (index--) {
    pos = -1;
    row = rows[index].children;
    out = [];

    while (++pos < alignLength) {
      out[pos] = self.all(row[pos]).join('');
    }

    result[index] = out.join('@');
  }

  pos = -1;

  while (++pos < alignLength) {
    alignHeading.push('cb');
    alignRow.push((align[pos] || 'l').charAt(0));
  }

  result = [].concat(
    [
      '',
      'tab(@) allbox;',
      alignHeading.join(' '),
      alignRow.join(' ') + ' .'
    ],
    result,
    ['.TE']
  ).join('\n');

  return macro('TS', result);
}

/* Compile a `root` node.  This compiles a man header,
 * and the children of `root`. */
function root(node) {
  return this.all(node).join('\n');
}
