import path from 'path';
import { builtinModules } from 'module';

const JS_EXTENSIONS = new Set(['.js', '.mjs']);

const baseURL = new URL('file://');
baseURL.pathname = process.cwd() + '/';

/**
 * @param {string} specifier
 * @param {{
 *     parentURL:(string|undefined),
 *     conditions:Array<string>,
 * }} context
 * @param {Function} defaultResolve
 * @returns {{
 *   url:string,
 * }}
 */
export function resolve(specifier, context, defaultResolve) {
  const { parentURL = baseURL } = context;
  if (builtinModules.includes(specifier)) {
    return {
      url: 'nodejs:' + specifier
    };
  }
  if (!(specifier.startsWith('file:') || /^\.{1,2}[/]/.test(specifier))) {
    // For node_modules support:
    // return defaultResolve(specifier, {parentURL}, defaultResolve);
    throw new Error(
      "Imports must either be URLs or begin with './' or '../'; " +
      `'${specifier}' satisfied neither of these requirements.`
    );
  }
  const resolved = new URL(specifier, parentURL);
  return {
    url: resolved.href
  };
}

/**
 * @param {string} url
 * // param {Object} context
 * // param {Function} defaultGetFormat
 * @returns {{
 *   format:string,
 * }}
 */
export function getFormat(url /* , context, defaultGetFormat */) {
  if (url.startsWith('nodejs:') && builtinModules.includes(url.slice(7))) {
    return {
      format: 'builtin'
    };
  }
  const { pathname } = new URL(url);
  const ext = path.extname(pathname);
  if (!JS_EXTENSIONS.has(ext)) {
    throw new Error(
      `Cannot load file with non-JavaScript file extension of '${ext}'.`);
  }
  return {
    format: 'module'
  };
}
