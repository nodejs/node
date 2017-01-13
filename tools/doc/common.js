'use strict';

const yaml = require('js-yaml');

function isYAMLBlock(text) {
  return !!text.match(/^<!-- YAML/);
}

exports.isYAMLBlock = isYAMLBlock;

function arrify(value) {
  return Array.isArray(value) ? value : [value];
}

function extractAndParseYAML(text) {
  text = text.trim();

  text = text.replace(/^<!-- YAML/, '')
             .replace(/-->$/, '');

  // js-yaml.safeLoad() throws on error
  const meta = yaml.safeLoad(text);

  const added = meta.added || meta.Added;
  if (added) {
    // Since semver-minors can trickle down to previous major versions,
    // features may have been added in multiple versions.
    meta.added = arrify(added);
  }

  const deprecated = meta.deprecated || meta.Deprecated;
  if (deprecated) {
    // Treat deprecated like added for consistency.
    meta.deprecated = arrify(deprecated);
  }

  return meta;
}

exports.extractAndParseYAML = extractAndParseYAML;
