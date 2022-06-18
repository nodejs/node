export function resolve(url, context, next) {
  const {
    format,
    url: nextUrl,
  } = next(url, context);

  return {
    format,
    url: `${nextUrl}?foo`,
  };
}
