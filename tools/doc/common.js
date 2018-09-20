'use strict';

const yaml =
  require(`${__dirname}/../node_modules/eslint/node_modules/js-yaml`);

function isYAMLBlock(text) {
  return /^<!-- YAML/.test(text);
}

function arrify(value) {
  return Array.isArray(value) ? value : [value];
}

function extractAndParseYAML(text) {
  text = text.trim()
             .replace(/^<!-- YAML/, '')
             .replace(/-->$/, '');

  // js-yaml.safeLoad() throws on error.
  const meta = yaml.safeLoad(text);

  if (meta.added) {
    // Since semver-minors can trickle down to previous major versions,
    // features may have been added in multiple versions.
    meta.added = arrify(meta.added);
  }

  if (meta.napiVersion) {
    meta.napiVersion = arrify(meta.napiVersion);
  }

  if (meta.deprecated) {
    // Treat deprecated like added for consistency.
    meta.deprecated = arrify(meta.deprecated);
  }

  if (meta.removed) {
    meta.removed = arrify(meta.removed);
  }

  meta.changes = meta.changes || [];

  return meta;
}

module.exports = { arrify, isYAMLBlock, extractAndParseYAML };
