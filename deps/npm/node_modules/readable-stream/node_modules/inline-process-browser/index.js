'use strict';
var through = require('through2');
var falafel = require('falafel');

module.exports = apply;
var regex = /process\s*\.\s*browser/;
function apply() {
  var buffers = [];

  return through(function(chunk, enc, next) {
    buffers.push(chunk);
    next();
  }, function(next) {
    var string = Buffer.concat(buffers).toString();
    if (!string.match(regex)) {
      this.push(string);
      return next();
    }
    var resp = falafel(string, {
      ecmaVersion: 6,
      allowReturnOutsideFunction: true
    }, function (node) {

      if (
        node.type === 'MemberExpression' &&
        node.object && node.property &&
        node.object.name === 'process'
        && node.property.name === 'browser' &&
        !(node.parent ? node.parent.operator === '=' && node.parent.left === node : true)
        ) {
        node.update('true');
      }
    });
    this.push(resp.toString());
    next();
  });
}
