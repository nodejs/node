// Export the list of imported modules. It's available after the |ready| promise
// is resolved.
export let importedModules = ['export-on-dynamic-import-script.js'];
export let ready = import('./export-on-load-script.js')
  .then(module => {
    Array.prototype.push.apply(importedModules, module.importedModules);
    return importedModules;
  });
