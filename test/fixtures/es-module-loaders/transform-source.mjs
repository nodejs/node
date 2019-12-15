export async function transformSource(url, source) {
  return source.replace(`'A message';`, `'A message'.toUpperCase();`);
}
