export async function transformSource(
  { url, format, source }, defaultTransformSource, loader) {
  if (source && source.replace) {
    return source.replace(`'A message';`, `'A message'.toUpperCase();`);
  } else { // source could be a buffer, e.g. for WASM
    return defaultTransformSource(
      { url, format, source }, defaultTransformSource, loader);
  }
}
