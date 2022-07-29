import { URL } from 'url';
import path from 'path';
import process from 'process';
import { builtinModules } from 'module';

const JS_EXTENSIONS = new Set(['.js', '.mjs']);

const baseURL = new URL('file://');
baseURL.pathname = process.cwd() + '/';

export function resolve(specifier, { parentURL = baseURL }, next) {
  if (builtinModules.includes(specifier)) {
    return {
      shortCircuit: true,
      url: 'node:' + specifier
    };
  }
  if (/^\.{1,2}[/]/.test(specifier) !== true && !specifier.startsWith('file:')) {
    // For node_modules support:
    // return next(specifier);
    throw new Error(
      `imports must be URLs or begin with './', or '../'; '${specifier}' does not`);
  }
  const resolved = new URL(specifier, parentURL);
  return {
    shortCircuit: true,
    url: resolved.href
  };
}

export function getFormat(url, context, defaultGetFormat) {
  if (url.startsWith('node:') && builtinModules.includes(url.slice(5))) {
    return {
      format: 'builtin'
    };
  }
  const { pathname } = new URL(url);
  const ext = path.extname(pathname);
  if (!JS_EXTENSIONS.has(ext)) {
    throw new Error(
      `Cannot load file with non-JavaScript file extension ${ext}.`);
  }
  return {
    format: 'module'
  };
}
