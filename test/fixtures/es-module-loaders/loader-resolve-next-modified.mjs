export async function resolve(url, context, next) {
  const {
    format,
    url: nextUrl,
  } = await next(url, context);

  return {
    format,
    url: `${nextUrl}?foo`,
  };
}
