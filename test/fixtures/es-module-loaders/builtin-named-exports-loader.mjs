import module from 'module';

const GET_BUILTIN = `$__get_builtin_hole_${Date.now()}`;

export function getGlobalPreloadCode() {
  return `\
Object.defineProperty(globalThis, ${JSON.stringify(GET_BUILTIN)}, {
  value: (builtinName) => {
    return getBuiltin(builtinName);
  },
  enumerable: false,
  configurable: false,
});
`;
}

export async function resolve(specifier, context, nextResolve) {
  const def = await nextResolve(specifier, context);
  if (def.url.startsWith('nodejs:')) {
    return {
      url: `custom-${def.url}`,
    };
  }
  return def;
}

export function getSource(url) {
  if (url.startsWith('custom-nodejs:')) {
    const urlObj = new URL(url);
    return {
      source: generateBuiltinModule(urlObj.pathname),
      format: 'module',
    };
  }
  return null;
}

export function getFormat(url) {
  if (url.startsWith('custom-nodejs:')) {
    return { format: 'module' };
  }
  return null;
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
