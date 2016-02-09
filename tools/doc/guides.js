var fs = require('fs');
var path = require('path');

var VFile = require('vfile');
var remark = require('remark');
var remarkHtml = require('remark-html');
var slug = require('remark-slug');
var toc = require('remark-toc');
// TODO(qard): includes get hijacked by preprocess in generate script
// var include = require('remark-include');

module.exports = toHTML;

function makeVFile (input, filepath) {
  var dir = path.dirname(filepath);
  var ext = path.extname(filepath);
  var name = path.basename(filepath, ext).slice(1);
  return new VFile({
    directory: dir,
    filename: name,
    extension: ext,
    contents: input
  });
}

function render (template, content, cb) {
  fs.readFile(template, function (err, res) {
    if (err) return cb(err);
    var body = res.toString();
    body = body.replace(/__CONTENT__/g, content);
    cb(null, body);
  });
}

function toHTML(input, filepath, template, cb) {
  var processor = remark();
  processor.use(toc);
  processor.use(slug);
  processor.use(remarkHtml);
  // TODO(qard): includes get hijacked by preprocess in generate script
  // processor.use(include, {
  //   cwd: 'doc/guides'
  // });

  var file = makeVFile(input, filepath);
  var content = processor.process(file);
  render(template, content, cb);
}
