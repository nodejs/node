'use strict';

const Buffer = require('buffer').Buffer;

exports.toBuf = function toBuf(str, encoding) {
  return typeof str === 'string' ?
    Buffer.from(str, encoding && encoding !== 'buffer' ? encoding : 'utf8') :
    str;
};

exports.encode = function(item, encoding, defaultEncoding) {
  encoding = encoding || defaultEncoding;
  return (encoding && encoding !== 'buffer') ? item.toString(encoding) : item;
};

const rechk = /^[0-9A-Z\-]+$/;
exports.filterDuplicates = function(names) {
  // Drop all-caps names in favor of their lowercase aliases,
  // for example, 'sha1' instead of 'SHA1'.
  var ctx = {};
  names.forEach((name) => {
    var key = name;
    if (rechk.test(key)) key = key.toLowerCase();
    if (!ctx.hasOwnProperty(key) || ctx[key] < name)
      ctx[key] = name;
  });

  return Object.getOwnPropertyNames(ctx).map((key) => {
    return ctx[key];
  }).sort();
};
