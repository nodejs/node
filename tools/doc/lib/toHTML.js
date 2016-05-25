module.exports = function toHTML(input, filename, template, nodeVersion, cb) {
  if (typeof nodeVersion === 'function') {
    cb = nodeVersion;
    nodeVersion = null;
  }
  nodeVersion = nodeVersion || process.version;

  if (gtocData) {
    return onGtocLoaded();
  }

  if (gtocLoading === null) {
    gtocLoading = [onGtocLoaded];
    return loadGtoc(function(err, data) {
      if (err) throw err;
      gtocData = data;
      gtocLoading.forEach(function(xs) {
        xs();
      });
    });
  }

  if (gtocLoading) {
    return gtocLoading.push(onGtocLoaded);
  }

  function onGtocLoaded() {
    var lexed = marked.lexer(input);
    fs.readFile(template, 'utf8', function(er, template) {
      if (er) return cb(er);
      render(lexed, filename, template, nodeVersion, cb);
    });
  }
}
