const DATA_URL_PATTERN = /^data:application\/json(?:[^,]*?)(;base64)?,([\s\S]*)$/;
const JSON_URL_PATTERN = /\.json(\?[^#]*)?(#.*)?$/;

export function resolve(url, context, next) {
  // Mutation from resolve hook should be discarded.
  context.importAssertions.type = 'whatever';
  return next(url);
}

export function load(url, context, next) {
  if (context.importAssertions.type == null &&
      (DATA_URL_PATTERN.test(url) || JSON_URL_PATTERN.test(url))) {
    const { importAssertions } = context;
    importAssertions.type = 'json';
  }
  return next(url);
}
