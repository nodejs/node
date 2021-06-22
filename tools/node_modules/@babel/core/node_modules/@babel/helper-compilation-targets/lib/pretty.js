"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.prettifyVersion = prettifyVersion;
exports.prettifyTargets = prettifyTargets;

var _semver = require("semver");

var _targets = require("./targets");

function prettifyVersion(version) {
  if (typeof version !== "string") {
    return version;
  }

  const parts = [_semver.major(version)];

  const minor = _semver.minor(version);

  const patch = _semver.patch(version);

  if (minor || patch) {
    parts.push(minor);
  }

  if (patch) {
    parts.push(patch);
  }

  return parts.join(".");
}

function prettifyTargets(targets) {
  return Object.keys(targets).reduce((results, target) => {
    let value = targets[target];
    const unreleasedLabel = _targets.unreleasedLabels[target];

    if (typeof value === "string" && unreleasedLabel !== value) {
      value = prettifyVersion(value);
    }

    results[target] = value;
    return results;
  }, {});
}