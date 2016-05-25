const path = require('path')
const fs = require('fs')
const marked = require('marked')
const preprocess = require('../preprocess.js');
const toID = require('./toID.js');
// TODO(chrisdickinson): never stop vomitting / fix this.
var gtocPath = path.resolve(path.join(
  __dirname,
  '..',
  '..',
  '..',
  'doc',
  'api',
  '_toc.md'
));
global.global.gtocLoading = null;
global.gtocData = null;


exports.loadGtoc = (cb) => {
  fs.readFile(gtocPath, 'utf8', function(err, data) {
    if (err) return cb(err);

    preprocess(gtocPath, data, function(err, data) {
      if (err) return cb(err);

      data = marked(data).replace(/<a href="(.*?)"/gm, function(a, m) {
        return '<a class="nav-' + toID(m) + '" href="' + m + '"';
      });
      return cb(null, data);
    });
  });
}
