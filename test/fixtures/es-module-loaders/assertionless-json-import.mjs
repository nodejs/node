const DATA_URL_PATTERN = /^data:application\/json(?:[^,]*?)(;base64)?,([\s\S]*)$/;
const JSON_URL_PATTERN = /^[^?]+\.json(\?[^#]*)?(#.*)?$/;

export async function resolve(specifier, context, next) {
  const noAttributesSpecified = context.importAttributes.type == null;

  // Mutation from resolve hook should be discarded.
  context.importAttributes.type = 'whatever';

  // This fixture assumes that no other resolve hooks in the chain will error on invalid import attributes
  // (as defaultResolve doesn't).
  const result = await next(specifier, context);

  if (noAttributesSpecified &&
      (DATA_URL_PATTERN.test(result.url) || JSON_URL_PATTERN.test(result.url))) {
    // Clean new import attributes object to ensure that this test isn't passing due to mutation.
    result.importAttributes = {
      ...(result.importAttributes ?? context.importAttributes),
      type: 'json',
    };
  }

  return result;
}
