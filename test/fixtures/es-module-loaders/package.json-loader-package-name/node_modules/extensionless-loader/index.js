const extensionlessPath = /\/[^.]+$/;

export async function resolve(specifier, context, next) {
  const n = await next(specifier.toString(), context);
  if (n.format == null && extensionlessPath.test(n.url)) {
    n.format = "module";
  }
  return n;
}
