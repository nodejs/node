'use strict';

var trim = require('trim');
var paramCase = require('kebab-case');
var information = require('property-information');
var spaces = require('space-separated-tokens');
var commas = require('comma-separated-tokens');
var nan = require('is-nan');
var is = require('unist-util-is');

module.exports = wrapper;

function wrapper(h, node, prefix) {
  var r;
  var v;

  if (typeof h !== 'function') {
    throw new Error('h is not a function');
  }

  r = react(h);
  v = vdom(h);

  if (prefix === null || prefix === undefined) {
    prefix = r === true || v === true ? 'h-' : false;
  }

  if (is('root', node)) {
    if (node.children.length === 1 && is('element', node.children[0])) {
      node = node.children[0];
    } else {
      node = {
        type: 'element',
        tagName: 'div',
        properties: {},
        children: node.children
      };
    }
  } else if (!is('element', node)) {
    throw new Error('Expected root or element, not `' + ((node && node.type) || node) + '`');
  }

  return toH(h, node, {
    prefix: prefix,
    key: 0,
    react: r,
    vdom: v,
    hyperscript: hyperscript(h)
  });
}

/* Transform a HAST node through a hyperscript interface
 * to *anything*! */
function toH(h, node, ctx) {
  var selector = node.tagName;
  var properties;
  var attributes;
  var children;
  var property;
  var elements;
  var length;
  var index;
  var value;

  properties = node.properties;
  attributes = {};

  for (property in properties) {
    addAttribute(attributes, property, properties[property], ctx);
  }

  if (ctx.vdom === true) {
    selector = selector.toUpperCase();
  }

  if (ctx.hyperscript === true && attributes.id) {
    selector += '#' + attributes.id;
    delete attributes.id;
  }

  if ((ctx.hyperscript === true || ctx.vdom === true) && attributes.className) {
    selector += '.' + spaces.parse(attributes.className).join('.');
    delete attributes.className;
  }

  if (typeof attributes.style === 'string') {
    /* VDOM expects a `string` style in `attributes`
     * See https://github.com/Matt-Esch/virtual-dom/blob/947ecf9/
     * docs/vnode.md#propertiesstyle-vs-propertiesattributesstyle */
    if (ctx.vdom === true) {
      if (!attributes.attributes) {
        attributes.attributes = {};
      }

      attributes.attributes.style = attributes.style;
      delete attributes.style;
    /* React only accepts `style` as object. */
    } else if (ctx.react === true) {
      attributes.style = parseStyle(attributes.style);
    }
  }

  if (ctx.prefix) {
    ctx.key++;
    attributes.key = ctx.prefix + ctx.key;
  }

  elements = [];
  children = node.children || [];
  length = children.length;
  index = -1;

  while (++index < length) {
    value = children[index];

    if (is('element', value)) {
      elements.push(toH(h, value, ctx));
    } else if (is('text', value)) {
      elements.push(value.value);
    }
  }

  /* Ensure no React warnings are triggered for
   * void elements having children passed in. */
  return elements.length === 0 ? h(selector, attributes) : h(selector, attributes, elements);
}

/* Add `name` and its `value` to `props`. */
function addAttribute(props, name, value, ctx) {
  var info = information(name) || {};
  var subprop;

  /* Ignore nully, `false`, `NaN`, and falsey known
   * booleans. */
  if (
    value === null ||
    value === undefined ||
    value === false ||
    nan(value) ||
    (info.boolean && !value)
  ) {
    return;
  }

  name = info.name || paramCase(name);

  if (value !== null && typeof value === 'object' && 'length' in value) {
    /* Accept `array`.  Most props are space-separater. */
    value = (info.commaSeparated ? commas : spaces).stringify(value);
  }

  /* Treat `true` and truthy known booleans. */
  if (info.boolean && ctx.hyperscript === true) {
    value = '';
  }

  if (info.name !== 'class' && (info.mustUseAttribute || !info.name)) {
    if (ctx.vdom === true) {
      subprop = 'attributes';
    } else if (ctx.hyperscript === true) {
      subprop = 'attrs';
    }

    if (subprop) {
      if (props[subprop] === undefined) {
        props[subprop] = {};
      }

      props[subprop][name] = value;

      return;
    }
  }

  props[info.propertyName || name] = value;
}

/* Check if `h` is `react.createElement`.  It doesn’t accept
 * `class` as an attribute, it must be added through the
 * `selector`. */
function react(h) {
  var node = h && h('div');
  return Boolean(node && ('_owner' in node || '_store' in node) && node.key === null);
}

/* Check if `h` is `hyperscript`.  It doesn’t accept
 * `class` as an attribute, it must be added through the
 * `selector`. */
function hyperscript(h) {
  return Boolean(h && h.context && h.cleanup);
}

/**
 * Check if `h` is `virtual-dom/h`.  It’s the only
 * hyperscript “compatible” interface needing `attributes`. */
function vdom(h) {
  try {
    return h('div').type === 'VirtualNode';
  } catch (err) { /* Empty */ }

  /* istanbul ignore next */
  return false;
}

function parseStyle(value) {
  var result = {};
  var declarations = value.split(';');
  var length = declarations.length;
  var index = -1;
  var declaration;
  var prop;
  var pos;

  while (++index < length) {
    declaration = declarations[index];
    pos = declaration.indexOf(':');
    if (pos !== -1) {
      prop = camelCase(trim(declaration.slice(0, pos)));
      result[prop] = trim(declaration.slice(pos + 1));
    }
  }

  return result;
}

function camelCase(val) {
  if (val.slice(0, 4) === '-ms-') {
    val = 'ms-' + val.slice(4);
  }

  return val.replace(/-([a-z])/g, replace);
}

function replace($0, $1) {
  return $1.toUpperCase();
}
