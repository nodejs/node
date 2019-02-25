import { URL } from 'url';
import { builtinModules } from 'module';

const baseURL = new URL('file://');
baseURL.pathname = process.cwd() + '/';

export function resolve (specifier, base = baseURL) {
  if (builtinModules.includes(specifier)) {
    return {
      url: specifier,
      format: 'builtin'
    };
  }
  // load all dependencies as esm, regardless of file extension
  const url = new URL(specifier, base).href;
  return {
    url,
    format: 'esm'
  };
}
