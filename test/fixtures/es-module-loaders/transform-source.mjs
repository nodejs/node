export async function transformSource(
  source, { url, format }, defaultTransformSource) {
  if (format === 'module') {
    if (typeof source !== 'string') {
      source = new TextDecoder().decode(source);
    }
    return {
      source: source.replace(`'A message';`, `'A message'.toUpperCase();`)
    };
  } else { // source could be a buffer, e.g. for WASM
    return defaultTransformSource(
      source, {url, format}, defaultTransformSource);
  }
}
