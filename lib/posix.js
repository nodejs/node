process.fs.Stats.prototype._checkModeProperty = function (property) {
  return ((this.mode & property) === property);
};

process.fs.Stats.prototype.isDirectory = function () {
  return this._checkModeProperty(process.S_IFDIR);
};

process.fs.Stats.prototype.isFile = function () {
  return this._checkModeProperty(process.S_IFREG);
};

process.fs.Stats.prototype.isBlockDevice = function () {
  return this._checkModeProperty(process.S_IFBLK);
};

process.fs.Stats.prototype.isCharacterDevice = function () {
  return this._checkModeProperty(process.S_IFCHR);
};

process.fs.Stats.prototype.isSymbolicLink = function () {
  return this._checkModeProperty(process.S_IFLNK);
};

process.fs.Stats.prototype.isFIFO = function () {
  return this._checkModeProperty(process.S_IFIFO);
};

process.fs.Stats.prototype.isSocket = function () {
  return this._checkModeProperty(process.S_IFSOCK);
};

for (var key in process.fs) {
  if (process.fs.hasOwnProperty(key)) exports[key] = process.fs[key];
}
