export async function transformSource(
  { url, format, source, defaultTransformSource }) {
  if (source && source.replace) {
    return {
      source: source.replace(`'A message';`, `'A message'.toUpperCase();`)
    };
  } else { // source could be a buffer, e.g. for WASM
    return defaultTransformSource(
      {url, format, source, defaultTransformSource});
  }
}
