var Buffer = process.binding('buffer').Buffer;

exports.Buffer = Buffer;

Buffer.prototype.toString = function () {
  return this.utf8Slice(0, this.length);
};

Buffer.prototype.toJSON = function () {
  return this.utf8Slice(0, this.length);
  /*
  var s = "";
  for (var i = 0; i < this.length; i++) {
    s += this[i].toString(16) + " ";
  }
  return s;
  */
};


