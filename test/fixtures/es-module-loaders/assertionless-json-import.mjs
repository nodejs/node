import { registerHooks } from 'node:module';

const DATA_URL_PATTERN = /^data:application\/json(?:[^,]*?)(;base64)?,([\s\S]*)$/;
const JSON_URL_PATTERN = /^[^?]+\.json(\?[^#]*)?(#.*)?$/;

function resolve(specifier, context, next) {
  // Hooks also run for require() calls, which have no import attributes.
  if (context.importAttributes === undefined) {
    return next(specifier, context);
  }

  const noAttributesSpecified = context.importAttributes.type == null;

  // This fixture assumes that no other resolve hooks in the chain will error on invalid import attributes
  // (as defaultResolve doesn't).
  const result = next(specifier, {
    ...context,
    importAttributes: { ...context.importAttributes, type: 'whatever' },
  });

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

registerHooks({ resolve });
