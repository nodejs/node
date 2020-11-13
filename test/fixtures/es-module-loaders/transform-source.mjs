export async function loadFromURL(url, context, defaultLoadFromURL) {
  let {format, source} = await defaultLoadFromURL(url, context);
  if (format === 'module') {
    if (typeof source !== 'string') {
      source = new TextDecoder().decode(source);
    }
    return {
      format,
      source: source.replace(`'A message';`, `'A message'.toUpperCase();`)
    };
  } else { // source could be a buffer, e.g. for WASM
    return {format, source};
  }
}
