module.exports = function parseYAML(text) {
  const meta = common.extractAndParseYAML(text);
  const html = ['<div class="api_metadata">'];

  if (meta.added) {
    html.push(`<span>Added in: ${meta.added.join(', ')}</span>`);
  }

  if (meta.deprecated) {
    html.push(`<span>Deprecated since: ${meta.deprecated.join(', ')} </span>`);
  }

  html.push('</div>');
  return html.join('\n');
}
