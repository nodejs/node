var fs = require('fs');
var path = require('path');
var marked = require('marked');

var doc = path.resolve(__dirname, '..', '..', 'doc', 'api', 'addons.markdown');
var verifyDir = path.resolve(__dirname, '..', '..', 'test', 'addons');

var contents = fs.readFileSync(doc).toString();

var tokens = marked.lexer(contents, {});
var files = null;
var id = 0;

// Just to make sure that all examples will be processed
tokens.push({ type: 'heading' });

var oldDirs = fs.readdirSync(verifyDir);
oldDirs = oldDirs.filter(function(dir) {
  return /^doc-/.test(dir);
}).map(function(dir) {
  return path.resolve(verifyDir, dir);
});

for (var i = 0; i < tokens.length; i++) {
  var token = tokens[i];
  if (token.type === 'heading') {
    if (files && Object.keys(files).length !== 0) {
      verifyFiles(files,
                  console.log.bind(null, 'wrote'),
                  function(err) { if (err) throw err; });
    }
    files = {};
  } else if (token.type === 'code') {
    var match = token.text.match(/^\/\/\s+(.*\.(?:cc|h|js))[\r\n]/);
    if (match === null)
      continue;
    files[match[1]] = token.text;
  }
}

function once(fn) {
  var once = false;
  return function() {
    if (once)
      return;
    once = true;
    fn.apply(this, arguments);
  };
}

function verifyFiles(files, onprogress, ondone) {
  var dir = path.resolve(verifyDir, 'doc-' + id++);

  files = Object.keys(files).map(function(name) {
    return {
      path: path.resolve(dir, name),
      name: name,
      content: files[name]
    };
  });
  files.push({
    path: path.resolve(dir, 'binding.gyp'),
    content: JSON.stringify({
      targets: [
        {
          target_name: 'addon',
          sources: files.map(function(file) {
            return file.name;
          })
        }
      ]
    })
  });

  fs.mkdir(dir, function() {
    // Ignore errors

    var done = once(ondone);
    var waiting = files.length;
    files.forEach(function(file) {
      fs.writeFile(file.path, file.content, function(err) {
        if (err)
          return done(err);

        if (onprogress)
          onprogress(file.path);

        if (--waiting === 0)
          done();
      });
    });
  });
}
