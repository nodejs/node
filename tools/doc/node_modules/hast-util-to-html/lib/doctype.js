'use strict';

module.exports = doctype;

/* Stringify a doctype `node`. */
function doctype(ctx, node) {
  var pub = node.public;
  var sys = node.system;
  var val = '<!DOCTYPE';

  if (!node.name) {
    return val + '>';
  }

  val += ' ' + node.name;

  if (pub != null) {
    val += ' PUBLIC ' + smart(pub);
  } else if (sys != null) {
    val += ' SYSTEM';
  }

  if (sys != null) {
    val += ' ' + smart(sys);
  }

  return val + '>';
}

function smart(value) {
  var quote = value.indexOf('"') === -1 ? '"' : '\'';
  return quote + value + quote;
}
