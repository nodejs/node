export async function transformSource(source, { url, format }) {
  if (format === 'module') {
    if (typeof source !== 'string') {
      source = new TextDecoder().decode(source);
    }
    return {
      source: source.replace(`'A message';`, `'A message'.toUpperCase();`)
    };
  }
  return { source };
}
