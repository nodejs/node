export async function transformSource(source, { url }) {
  if (url.endsWith('test-loader-chaining.mjs')) {
    return {
      source: source.toString().replace(/B/g, 'C'),
    };
  }
  return { source };
}
