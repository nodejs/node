import * as module from './export-on-load-script.js';
const filename = 'export-on-static-import-script.js';
export const importedModules = [filename].concat(module.importedModules);
