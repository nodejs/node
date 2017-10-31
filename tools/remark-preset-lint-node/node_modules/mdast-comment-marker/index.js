'use strict';

/* Expose. */
module.exports = marker;

/* HTML type. */
var T_HTML = 'html';

/* Expression for eliminating extra spaces */
var SPACES = /\s+/g;

/* Expression for parsing parameters. */
var PARAMETERS = new RegExp(
  '\\s+' +
  '(' +
    '[-a-z0-9_]+' +
  ')' +
  '(?:' +
    '=' +
    '(?:' +
      '"' +
      '(' +
        '(?:' +
          '\\\\[\\s\\S]' +
          '|' +
          '[^"]' +
        ')+' +
      ')' +
      '"' +
      '|' +
      '\'' +
      '(' +
        '(?:' +
          '\\\\[\\s\\S]' +
          '|' +
          '[^\']' +
        ')+' +
      ')' +
      '\'' +
      '|' +
      '(' +
        '(?:' +
          '\\\\[\\s\\S]' +
          '|' +
          '[^"\'\\s]' +
        ')+' +
      ')' +
    ')' +
  ')?',
  'gi'
);

var MARKER = new RegExp(
  '(' +
    '\\s*' +
    '<!--' +
    '\\s*' +
    '([a-zA-Z0-9-]+)' +
    '(\\s+([\\s\\S]*?))?' +
    '\\s*' +
    '-->' +
    '\\s*' +
  ')'
);

/* Parse a comment marker */
function marker(node) {
  var value;
  var match;
  var params;

  if (!node || node.type !== T_HTML) {
    return null;
  }

  value = node.value;
  match = value.match(MARKER);

  if (!match || match[1].length !== value.length) {
    return null;
  }

  params = parameters(match[3] || '');

  if (!params) {
    return null;
  }

  return {
    name: match[2],
    attributes: match[4] || '',
    parameters: params,
    node: node
  };
}

/* eslint-disable max-params */

/* Parse `value` into an object. */
function parameters(value) {
  var attributes = {};
  var rest = value.replace(PARAMETERS, function ($0, $1, $2, $3, $4) {
    var result = $2 || $3 || $4 || '';

    if (result === 'true' || result === '') {
      result = true;
    } else if (result === 'false') {
      result = false;
    } else if (!isNaN(result)) {
      result = Number(result);
    }

    attributes[$1] = result;

    return '';
  });

  return rest.replace(SPACES, '') ? null : attributes;
}
