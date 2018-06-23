'use strict';

var xtend = require('xtend');
var spaces = require('space-separated-tokens').stringify;
var commas = require('comma-separated-tokens').stringify;
var information = require('property-information');
var entities = require('stringify-entities');
var kebab = require('kebab-case');
var ccount = require('ccount');
var all = require('./all');

module.exports = element;

/* Constants. */
var DATA = 'data';
var EMPTY = '';

/* Characters. */
var SPACE = ' ';
var DQ = '"';
var SQ = '\'';
var EQ = '=';
var LT = '<';
var GT = '>';
var SO = '/';

/* Stringify an element `node`. */
function element(ctx, node, index, parent) {
  var name = node.tagName;
  var content = all(ctx, name === 'template' ? node.content : node);
  var selfClosing = ctx.voids.indexOf(name.toLowerCase()) !== -1;
  var attrs = attributes(ctx, node.properties);
  var omit = ctx.omit;
  var value = '';

  /* If the node is categorised as void, but it has
   * children, remove the categorisation.  This
   * enables for example `menuitem`s, which are
   * void in W3C HTML but not void in WHATWG HTML, to
   * be stringified properly. */
  selfClosing = content ? false : selfClosing;

  if (attrs || !omit || !omit.opening(node, index, parent)) {
    value = LT + name + (attrs ? SPACE + attrs : EMPTY);

    if (selfClosing && ctx.close) {
      if (!ctx.tightClose || attrs.charAt(attrs.length - 1) === SO) {
        value += SPACE;
      }

      value += SO;
    }

    value += GT;
  }

  value += content;

  if (!selfClosing && (!omit || !omit.closing(node, index, parent))) {
    value += LT + SO + name + GT;
  }

  return value;
}

/* Stringify all attributes. */
function attributes(ctx, props) {
  var values = [];
  var key;
  var value;
  var result;
  var length;
  var index;
  var last;

  for (key in props) {
    value = props[key];

    if (value == null) {
      continue;
    }

    result = attribute(ctx, key, value);

    if (result) {
      values.push(result);
    }
  }

  length = values.length;
  index = -1;

  while (++index < length) {
    result = values[index];
    last = ctx.tight && result.charAt(result.length - 1);

    /* In tight mode, don’t add a space after quoted attributes. */
    if (index !== length - 1 && last !== DQ && last !== SQ) {
      values[index] = result + SPACE;
    }
  }

  return values.join(EMPTY);
}

/* Stringify one attribute. */
function attribute(ctx, key, value) {
  var info = information(key) || {};
  var name;

  if (
    value == null ||
    (typeof value === 'number' && isNaN(value)) ||
    (!value && info.boolean) ||
    (value === false && info.overloadedBoolean)
  ) {
    return EMPTY;
  }

  name = attributeName(ctx, key);

  if ((value && info.boolean) || (value === true && info.overloadedBoolean)) {
    return name;
  }

  return name + attributeValue(ctx, key, value);
}

/* Stringify the attribute name. */
function attributeName(ctx, key) {
  var info = information(key) || {};
  var name = info.name || kebab(key);

  if (
    name.slice(0, DATA.length) === DATA &&
    /[0-9]/.test(name.charAt(DATA.length))
  ) {
    name = DATA + '-' + name.slice(4);
  }

  return entities(name, xtend(ctx.entities, {
    subset: ctx.NAME
  }));
}

/* Stringify the attribute value. */
function attributeValue(ctx, key, value) {
  var info = information(key) || {};
  var options = ctx.entities;
  var quote = ctx.quote;
  var alternative = ctx.alternative;
  var unquoted;

  if (typeof value === 'object' && 'length' in value) {
    /* `spaces` doesn’t accept a second argument, but it’s
     * given here just to keep the code cleaner. */
    value = (info.commaSeparated ? commas : spaces)(value, {
      padLeft: !ctx.tightLists
    });
  }

  value = String(value);

  if (value || !ctx.collapseEmpty) {
    unquoted = value;

    /* Check unquoted value. */
    if (ctx.unquoted) {
      unquoted = entities(value, xtend(options, {
        subset: ctx.UNQUOTED,
        attribute: true
      }));
    }

    /* If `value` contains entities when unquoted... */
    if (!ctx.unquoted || unquoted !== value) {
      /* If the alternative is less common than `quote`, switch. */
      if (
        alternative &&
        ccount(value, quote) > ccount(value, alternative)
      ) {
        quote = alternative;
      }

      value = entities(value, xtend(options, {
        subset: quote === SQ ? ctx.SINGLE_QUOTED : ctx.DOUBLE_QUOTED,
        attribute: true
      }));

      value = quote + value + quote;
    }

    /* Don’t add a `=` for unquoted empties. */
    value = value ? EQ + value : value;
  }

  return value;
}
