var recast = require('recast');
var stream = require('stream');
var util = require('util');

var transformer = require('./unreachableBranchTransformer');

module.exports = UBT;
util.inherits(UBT, stream.Transform);

function UBT(file, opts) {
  if (!(this instanceof UBT)) {
    return UBT.configure(opts)(file);
  }

  stream.Transform.call(this);
  this._data = '';
}

UBT.prototype._transform = function(buf, enc, cb) {
  this._data += buf;
  cb();
};

UBT.prototype._flush = function(cb) {
  try {
    var code = UBT.transform(this._data);
    this.push(code);
  } catch(err) {
    this.emit('error', err);
    return;
  }
  cb();
};

UBT.configure = function(opts) {
  var ignores = ['.json'].concat(opts && opts.ignore || []);

  return function(file) {
    for (var i = 0; i < ignores.length; i++) {
      if (endsWith(file, ignores[i])) {
        return stream.PassThrough();
      }
    }

    return new UBT(file);
  }
};

UBT.transform = function(code) {
  var ast = transformer(recast.parse(code));
  return recast.print(ast).code;
};

function endsWith(str, suffix) {
  if (typeof str !== 'string' || typeof suffix !== 'string') {
    return false;
  }
  return str.indexOf(suffix, str.length - suffix.length) !== -1;
}
