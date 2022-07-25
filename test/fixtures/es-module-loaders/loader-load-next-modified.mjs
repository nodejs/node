export async function load(url, context, next) {
  const {
    format,
    source,
  } = await next(url, context);

  return {
    format,
    source: source + 1,
  };
}
