process.Stats.prototype._checkModeProperty = function (property) {
  return ((this.mode & property) === property);
};

process.Stats.prototype.isDirectory = function () {
  return this._checkModeProperty(process.S_IFDIR);
};

process.Stats.prototype.isFile = function () {
  return this._checkModeProperty(process.S_IFREG);
};

process.Stats.prototype.isBlockDevice = function () {
  return this._checkModeProperty(process.S_IFBLK);
};

process.Stats.prototype.isCharacterDevice = function () {
  return this._checkModeProperty(process.S_IFCHR);
};

process.Stats.prototype.isSymbolicLink = function () {
  return this._checkModeProperty(process.S_IFLNK);
};

process.Stats.prototype.isFIFO = function () {
  return this._checkModeProperty(process.S_IFIFO);
};

process.Stats.prototype.isSocket = function () {
  return this._checkModeProperty(process.S_IFSOCK);
};

exports.Stats = process.Stats;

for (var key in process.fs) {
  if (process.fs.hasOwnProperty(key)) exports[key] = process.fs[key];
}
