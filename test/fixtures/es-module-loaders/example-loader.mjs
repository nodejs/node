import { default as Module } from 'module';
import { extname } from 'path';
import { pathToFileURL } from 'url';

/**
 * @param {string} specifier
 * @param {!(URL|string|undefined)} parentModuleURL
//  * param {Function} defaultResolver
 * @returns {Promise<{url: string, format: string}>}
 */
export async function resolve(
  specifier,
  parentModuleURL = undefined
  // defaultResolver
) {
  const JS_EXTENSIONS = new Set(['.js', '.mjs']);

  if (parentModuleURL === undefined) {
    parentModuleURL = pathToFileURL(process.cwd()).href;
  }

  if (Module.builtinModules.includes(specifier)) {
    return {
      url: 'nodejs:' + specifier
    };
  }

  if (!(specifier.startsWith('file:') || /^\.{1,2}[/]/.test(specifier))) {
    // For node_modules support:
    // return defaultResolver(specifier, parentModuleURL);
    throw new Error(
      "Imports must either be URLs or begin with './' or '../'; " +
      `'${specifier}' satisfied neither of these requirements.`
    );
  }

  const resolved = new URL(specifier, parentModuleURL);
  const ext = extname(resolved.pathname);

  if (!JS_EXTENSIONS.has(ext)) {
    throw new Error(
      `Cannot load file with non-JavaScript file extension ${ext}.`);
  }

  return {
    format: 'module'
  };
}
