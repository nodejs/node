const path = require('path')
const marked = require('marked')
const getSection = require('./getSection')
const parseText = require('./parseText')
const parseLists = require('./parseLists')
const buildToc = require('./buildToc')
const toID = require('./toID.js');

module.exports = function render(lexed, filename, template, nodeVersion, cb) {
  if (typeof nodeVersion === 'function') {
    cb = nodeVersion;
    nodeVersion = null;
  }

  nodeVersion = nodeVersion || process.version;

  // get the section
  var section = getSection(lexed);

  filename = path.basename(filename, '.md');

  parseText(lexed);
  lexed = parseLists(lexed);

  // generate the table of contents.
  // this mutates the lexed contents in-place.
  buildToc(lexed, filename, function(er, toc) {
    if (er) return cb(er);

    var id = toID(path.basename(filename));

    template = template.replace(/__ID__/g, id);
    template = template.replace(/__FILENAME__/g, filename);
    template = template.replace(/__SECTION__/g, section);
    template = template.replace(/__VERSION__/g, nodeVersion);
    template = template.replace(/__TOC__/g, toc);
    template = template.replace(
      /__GTOC__/g,
      global.gtocData.replace('class="nav-' + id, 'class="nav-' + id + ' active')
    );

    // content has to be the last thing we do with
    // the lexed tokens, because it's destructive.
    const content = marked.parser(lexed);
    template = template.replace(/__CONTENT__/g, content);

    cb(null, template);
  });
}
