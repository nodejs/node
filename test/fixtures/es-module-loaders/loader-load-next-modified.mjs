export async function load(url, context, next) {
  const {
    format,
    source,
  } = await next(url);

  return {
    format,
    source: source + 1,
  };
}
