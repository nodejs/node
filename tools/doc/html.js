'use strict';

const common = require('./common.js');
const fs = require('fs');
const marked = require('marked');
const path = require('path');
const preprocess = require('./preprocess.js');
const typeParser = require('./type-parser.js');

const linkManPages = require('./lib/linkManPages')
const parseText = require('./lib/parseText')
const toHTML = require('./lib/toHTML')
const loadGtoc = require('./lib/loadGtoc')
const toID = require('./lib/toID')
const render = require('./lib/render')
const parseLists = require('./lib/parseLists')
const parseYAML = require('./lib/parseYAML')
const linkJsTypeDocs = require('./lib/linkJsTypeDocs')
const parseAPIHeader = require('./lib/parseAPIHeader')
const getSection = require('./lib/getSection')
const buildToc = require('./lib/buildToc')

module.exports = toHTML;

// customized heading without id attribute
var renderer = new marked.Renderer();
renderer.heading = function(text, level) {
  return '<h' + level + '>' + text + '</h' + level + '>\n';
};
marked.setOptions({
  renderer: renderer
});


var idCounters = {};
function getId(text) {
  text = text.toLowerCase();
  text = text.replace(/[^a-z0-9]+/g, '_');
  text = text.replace(/^_+|_+$/, '');
  text = text.replace(/^([^a-z])/, '_$1');
  if (idCounters.hasOwnProperty(text)) {
    text += '_' + (++idCounters[text]);
  } else {
    idCounters[text] = 0;
  }
  return text;
}
