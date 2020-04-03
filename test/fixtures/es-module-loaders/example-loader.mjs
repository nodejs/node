import { extname } from 'path';
import { default as Module } from 'module';

const JS_EXTENSIONS = new Set(['.js', '.mjs']);
const baseURL = new URL('file://');
baseURL.pathname = process.cwd() + '/';

/**
 * @param {!(string)} specifier
 * @param {{
 *   parentURL:!(URL|string|undefined),
 * }} context
 * @param {!(Function)} defaultResolve
 * @returns {{
 *   url:!(string),
 * }}
 */
export function resolve(specifier, { parentURL = baseURL }, defaultResolve) {
  if (Module.builtinModules.includes(specifier)) {
    return {
      url: 'nodejs:' + specifier,
    };
  }
  if (
    /^\.{1,2}[/]/.test(specifier) !== true &&
    !specifier.startsWith('file:')
  ) {
    // For node_modules support:
    // return defaultResolve(specifier, {parentURL}, defaultResolve);
    throw new Error(
      "Imports must either be URLs or begin with './' or '../'; " +
        `'${specifier}' satisfied neither of these requirements.`
    );
  }
  const resolved = new URL(specifier, parentURL);
  return {
    url: resolved.href,
  };
}

/**
 * @param {!(string)} url
// * param {!(Object)} context
// * param {!(Function)} defaultGetFormat
 * @returns {{
 *   format:!(string),
 * }}
 */
export function getFormat(url /* , context, defaultGetFormat */) {
  if (url.startsWith('nodejs:') &&
    Module.builtinModules.includes(url.slice(7))) {
    return {
      format: 'builtin',
    };
  }
  const { pathname } = new URL(url);
  const ext = extname(pathname);
  if (!JS_EXTENSIONS.has(ext)) {
    throw new Error(
      `Cannot load file with non-JavaScript file extension ${ext}.`
    );
  }
  return {
    format: 'module',
  };
}
