"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = mergeSourceMap;

function _remapping() {
  const data = require("@ampproject/remapping");

  _remapping = function () {
    return data;
  };

  return data;
}

function mergeSourceMap(inputMap, map, source) {
  const outputSources = map.sources;
  let result;

  if (outputSources.length > 1) {
    const index = outputSources.indexOf(source);

    if (index === -1) {
      result = emptyMap(inputMap);
    } else {
      result = mergeMultiSource(inputMap, map, index);
    }
  } else {
    result = mergeSingleSource(inputMap, map);
  }

  if (typeof inputMap.sourceRoot === "string") {
    result.sourceRoot = inputMap.sourceRoot;
  }

  return result;
}

function mergeSingleSource(inputMap, map) {
  return _remapping()([rootless(map), rootless(inputMap)], () => null);
}

function mergeMultiSource(inputMap, map, index) {
  map.sources[index] = "";
  let count = 0;
  return _remapping()(rootless(map), () => {
    if (count++ === index) return rootless(inputMap);
    return null;
  });
}

function emptyMap(inputMap) {
  var _inputMap$sourcesCont;

  const inputSources = inputMap.sources;
  const sources = [];
  const sourcesContent = (_inputMap$sourcesCont = inputMap.sourcesContent) == null ? void 0 : _inputMap$sourcesCont.filter((content, i) => {
    if (typeof content !== "string") return false;
    sources.push(inputSources[i]);
    return true;
  });
  return Object.assign({}, inputMap, {
    sources,
    sourcesContent,
    mappings: ""
  });
}

function rootless(map) {
  return Object.assign({}, map, {
    sourceRoot: null
  });
}