export async function transformSource(
  source, { url, format }, defaultTransformSource) {
  if (source && source.replace) {
    return {
      source: source.replace(`'A message';`, `'A message'.toUpperCase();`)
    };
  } else { // source could be a buffer, e.g. for WASM
    return defaultTransformSource(
      source, {url, format}, defaultTransformSource);
  }
}
