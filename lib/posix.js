node.fs.Stats.prototype._checkModeProperty = function (property) {
  return ((this.mode & property) === property);
};

node.fs.Stats.prototype.isDirectory = function () {
  return this._checkModeProperty(node.S_IFDIR);
};

node.fs.Stats.prototype.isFile = function () {
  return this._checkModeProperty(node.S_IFREG);
};

node.fs.Stats.prototype.isBlockDevice = function () {
  return this._checkModeProperty(node.S_IFBLK);
};

node.fs.Stats.prototype.isCharacterDevice = function () {
  return this._checkModeProperty(node.S_IFCHR);
};

node.fs.Stats.prototype.isSymbolicLink = function () {
  return this._checkModeProperty(node.S_IFLNK);
};

node.fs.Stats.prototype.isFIFO = function () {
  return this._checkModeProperty(node.S_IFIFO);
};

node.fs.Stats.prototype.isSocket = function () {
  return this._checkModeProperty(node.S_IFSOCK);
};

for (var key in node.fs) {
  if (node.fs.hasOwnProperty(key)) exports[key] = node.fs[key];
}
