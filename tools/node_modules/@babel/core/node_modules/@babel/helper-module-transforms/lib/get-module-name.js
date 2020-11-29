"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = getModuleName;

function getModuleName(rootOpts, pluginOpts) {
  var _pluginOpts$moduleRoo, _rootOpts$moduleIds, _rootOpts$moduleRoot;

  const {
    filename,
    filenameRelative = filename,
    sourceRoot = (_pluginOpts$moduleRoo = pluginOpts.moduleRoot) != null ? _pluginOpts$moduleRoo : rootOpts.moduleRoot
  } = rootOpts;
  const {
    moduleId = rootOpts.moduleId,
    moduleIds = (_rootOpts$moduleIds = rootOpts.moduleIds) != null ? _rootOpts$moduleIds : !!moduleId,
    getModuleId = rootOpts.getModuleId,
    moduleRoot = (_rootOpts$moduleRoot = rootOpts.moduleRoot) != null ? _rootOpts$moduleRoot : sourceRoot
  } = pluginOpts;
  if (!moduleIds) return null;

  if (moduleId != null && !getModuleId) {
    return moduleId;
  }

  let moduleName = moduleRoot != null ? moduleRoot + "/" : "";

  if (filenameRelative) {
    const sourceRootReplacer = sourceRoot != null ? new RegExp("^" + sourceRoot + "/?") : "";
    moduleName += filenameRelative.replace(sourceRootReplacer, "").replace(/\.(\w*?)$/, "");
  }

  moduleName = moduleName.replace(/\\/g, "/");

  if (getModuleId) {
    return getModuleId(moduleName) || moduleName;
  } else {
    return moduleName;
  }
}