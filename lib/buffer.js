var Buffer = process.binding('buffer').Buffer;

exports.Buffer = Buffer;

function toHex (n) {
  if (n < 16) return "0" + n.toString(16);
  return n.toString(16);
}

Buffer.prototype.toString = function () {
  var s = "<Buffer ";
  for (var i = 0; i < this.length; i++) {
    s += toHex(this[i]);
    if (i != this.length - 1) s += ' ';
  }
  s += ">";
  return s;
};

Buffer.prototype.toJSON = function () {
  return this.utf8Slice(0, this.length);
};


