
exports.join = function () {
  return exports.normalize(Array.prototype.join.call(arguments, "/"));
};

exports.normalizeArray = function (parts, keepBlanks) {
  var directories = [], prev;
  for (var i = 0, l = parts.length - 1; i <= l; i++) {
    var directory = parts[i];

    // if it's blank, but it's not the first thing, and not the last thing, skip it.
    if (directory === "" && i !== 0 && i !== l && !keepBlanks) continue;

    // if it's a dot, and there was some previous dir already, then skip it.
    if (directory === "." && prev !== undefined) continue;

    if (
      directory === ".."
      && directories.length
      && prev !== ".."
      && prev !== "."
      && prev !== undefined
      && (prev !== "" || keepBlanks)
    ) {
      directories.pop();
      prev = directories.slice(-1)[0]
    } else {
      if (prev === ".") directories.pop();
      directories.push(directory);
      prev = directory;
    }
  }
  return directories;
};

exports.normalize = function (path, keepBlanks) {
  return exports.normalizeArray(path.split("/"), keepBlanks).join("/");
};

exports.dirname = function (path) {
  // Can't strip trailing slashes since module.js incorrectly thinks
  // dirname('/a/b/') should yield '/a/b' instead of '/a'.
  // if (path.length > 1 && '/' === path[path.length-1]) {
  //  path = path.replace(/\/+$/, '');
  // }
  var lastSlash = path.lastIndexOf('/');
  switch (lastSlash) {
    case -1:
      return '.';
    case 0:
      return '/';
    default:
      return path.substring(0, lastSlash);
  }
};

exports.filename = function () {
  throw new Error("path.filename is deprecated. Please use path.basename instead.");
};
exports.basename = function (path, ext) {
  var f = path.substr(path.lastIndexOf("/") + 1);
  if (ext && f.substr(-1 * ext.length) === ext) {
    f = f.substr(0, f.length - ext.length);
  }
  return f;
};

exports.extname = function (path) {
  var index = path.lastIndexOf('.');
  return index < 0 ? '' : path.substring(index);
};

exports.exists = function (path, callback) {
  process.binding('fs').stat(path, function (err, stats) {
    if (callback) callback(err ? false : true);
  });
};
