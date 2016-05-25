const fs = require('fs')
const loadGtoc = require('./loadGtoc').loadGtoc
const render = require('./render')
const marked = require('marked')

// customized heading without id attribute
var renderer = new marked.Renderer();
renderer.heading = function(text, level) {
  return '<h' + level + '>' + text + '</h' + level + '>\n';
};
marked.setOptions({
  renderer: renderer
});


module.exports = function toHTML(input, filename, template, nodeVersion, cb) {
  if (typeof nodeVersion === 'function') {
    cb = nodeVersion;
    nodeVersion = null;
  }
  nodeVersion = nodeVersion || process.version;

  if (global.gtocData) {
    return onGtocLoaded();
  }

  if (global.gtocLoading === null) {
    global.gtocLoading = [onGtocLoaded];
    return loadGtoc(function(err, data) {
      if (err) throw err;
      global.gtocData = data;
      global.gtocLoading.forEach(function(xs) {
        xs();
      });
    });
  }

  if (global.gtocLoading) {
    return global.gtocLoading.push(onGtocLoaded);
  }

  function onGtocLoaded() {
    var lexed = marked.lexer(input);
    fs.readFile(template, 'utf8', function(er, template) {
      if (er) return cb(er);
      render(lexed, filename, template, nodeVersion, cb);
    });
  }
}
