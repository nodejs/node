import module from 'node:module';

/** @type {string} */
let GET_BUILTIN;
export function initialize(data) {
  GET_BUILTIN = data.GET_BUILTIN;
}

export async function resolve(specifier, context, next) {
  const def = await next(specifier, context);

  if (def.url.startsWith('node:')) {
    return {
      shortCircuit: true,
      url: `custom-${def.url}`,
      importAttributes: context.importAttributes,
    };
  }
  return def;
}

export function load(url, context, next) {
  if (url.startsWith('custom-node:')) {
    const urlObj = new URL(url);
    return {
      shortCircuit: true,
      source: generateBuiltinModule(urlObj.pathname),
      format: 'module',
    };
  }
  return next(url);
}

function generateBuiltinModule(builtinName) {
  const builtinInstance = module._load(builtinName);
  const builtinExports = [
    ...Object.keys(builtinInstance),
  ];
  return `\
const $builtinInstance = ${GET_BUILTIN}(${JSON.stringify(builtinName)});

export const __fromLoader = true;

export default $builtinInstance;

${
  builtinExports
    .map(name => `export const ${name} = $builtinInstance.${name};`)
    .join('\n')
}
`;
}
