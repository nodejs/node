'use strict';

var parseSelector = require('hast-util-parse-selector');
var camelcase = require('camelcase');
var propertyInformation = require('property-information');
var spaces = require('space-separated-tokens').parse;
var commas = require('comma-separated-tokens').parse;

module.exports = h;

/* Hyperscript compatible DSL for creating virtual HAST
 * trees. */
function h(selector, properties, children) {
  var node = parseSelector(selector);
  var property;

  if (
    properties &&
    !children &&
    (
      typeof properties === 'string' ||
      'length' in properties ||
      isNode(node.tagName, properties)
    )
  ) {
    children = properties;
    properties = null;
  }

  if (properties) {
    for (property in properties) {
      addProperty(node.properties, property, properties[property]);
    }
  }

  addChild(node.children, children);

  if (node.tagName === 'template') {
    node.content = {type: 'root', children: node.children};
    node.children = [];
  }

  return node;
}

/* Check if `value` is a valid child node of `tagName`. */
function isNode(tagName, value) {
  var type = value.type;

  if (typeof type === 'string') {
    type = type.toLowerCase();
  }

  if (tagName === 'input' || !type || typeof type !== 'string') {
    return false;
  }

  if (typeof value.children === 'object' && 'length' in value.children) {
    return true;
  }

  if (tagName === 'button') {
    return type !== 'menu' &&
      type !== 'submit' &&
      type !== 'reset' &&
      type !== 'button';
  }

  return 'value' in value;
}

/* Add `value` as a child to `nodes`. */
function addChild(nodes, value) {
  var index;
  var length;

  if (value === null || value === undefined) {
    return;
  }

  if (typeof value === 'string' || typeof value === 'number') {
    value = {type: 'text', value: String(value)};
  }

  if (typeof value === 'object' && 'length' in value) {
    index = -1;
    length = value.length;

    while (++index < length) {
      addChild(nodes, value[index]);
    }

    return;
  }

  if (typeof value !== 'object' || !('type' in value)) {
    throw new Error('Expected node, nodes, or string, got `' + value + '`');
  }

  nodes.push(value);
}

/* Add `name` and its `value` to `properties`. `properties` can
 * be prefilled by `parseSelector`: it can have `id` and `className`
 * properties. */
function addProperty(properties, name, value) {
  var info = propertyInformation(name) || {};
  var result = value;
  var key;

  /* Ignore nully and NaN values. */
  if (value === null || value === undefined || value !== value) {
    return;
  }

  /* Handle values. */
  if (name === 'style') {
    /* Accept `object`. */
    if (typeof value !== 'string') {
      result = [];

      for (key in value) {
        result.push([key, value[key]].join(': '));
      }

      result = result.join('; ');
    }
  } else if (info.spaceSeparated) {
    /* Accept both `string` and `Array`. */
    result = typeof value === 'string' ? spaces(result) : result;

    /* Class-names (which can be added both on
     * the `selector` and here). */
    if (name === 'class' && properties.className) {
      result = properties.className.concat(result);
    }
  } else if (info.commaSeparated) {
    /* Accept both `string` and `Array`. */
    result = typeof value === 'string' ? commas(result) : result;
  }

  result = parsePrimitive(info, name, result);

  properties[info.propertyName || camelcase(name)] = result;
}

/* Parse a (list of) primitives. */
function parsePrimitive(info, name, value) {
  var result = value;
  var index;
  var length;

  if (typeof value === 'object' && 'length' in value) {
    length = value.length;
    index = -1;
    result = [];

    while (++index < length) {
      result[index] = parsePrimitive(info, name, value[index]);
    }

    return result;
  }

  if (info.numeric || info.positiveNumeric) {
    if (!isNaN(result) && result !== '') {
      result = Number(result);
    }
  } else if (info.boolean || info.overloadedBoolean) {
    /* Accept `boolean` and `string`. */
    if (
      typeof result === 'string' &&
      (result === '' || value.toLowerCase() === name)
    ) {
      result = true;
    }
  }

  return result;
}
