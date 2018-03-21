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
  text = text.trim()
             .replace(/^<!-- YAML/, '')
             .replace(/-->$/, '');

  // js-yaml.safeLoad() throws on error
  const meta = yaml.safeLoad(text);

  if (meta.added) {
    // Since semver-minors can trickle down to previous major versions,
    // features may have been added in multiple versions.
    meta.added = arrify(meta.added);
  }

  if (meta.deprecated) {
    // Treat deprecated like added for consistency.
    meta.deprecated = arrify(meta.deprecated);
  }

  meta.changes = meta.changes || [];
  for (const entry of meta.changes) {
    entry.description = entry.description.replace(/^\^\s*/, '');
  }

  return meta;
}

exports.extractAndParseYAML = extractAndParseYAML;
