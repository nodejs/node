'use strict';

const yaml = require('js-yaml');

function isYAMLBlock(text) {
  return !!text.match(/^<!-- YAML/);
}

exports.isYAMLBlock = isYAMLBlock;

function extractAndParseYAML(text) {
  text = text.trim();

  text = text.replace(/^<!-- YAML/, '')
             .replace(/-->$/, '');

  // js-yaml.safeLoad() throws on error
  return yaml.safeLoad(text);
}

exports.extractAndParseYAML = extractAndParseYAML;
