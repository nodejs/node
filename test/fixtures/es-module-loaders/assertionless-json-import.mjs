const DATA_URL_PATTERN = /^data:application\/json(?:[^,]*?)(;base64)?,([\s\S]*)$/;
const JSON_URL_PATTERN = /^[^?]+\.json(\?[^#]*)?(#.*)?$/;

export async function resolve(specifier, context, next) {
  const noAssertionSpecified = context.importAssertions.type == null;

  // Mutation from resolve hook should be discarded.
  context.importAssertions.type = 'whatever';

  // This fixture assumes that no other resolve hooks in the chain will error on invalid import assertions
  // (as defaultResolve doesn't).
  const result = await next(specifier, context);

  if (noAssertionSpecified &&
      (DATA_URL_PATTERN.test(result.url) || JSON_URL_PATTERN.test(result.url))) {
    // Clean new import assertions object to ensure that this test isn't passing due to mutation.
    result.importAssertions = {
      ...(result.importAssertions ?? context.importAssertions),
      type: 'json',
    };
  }

  return result;
}
