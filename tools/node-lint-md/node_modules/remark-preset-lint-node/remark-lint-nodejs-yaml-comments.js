import yaml from "js-yaml";
import { visit } from "unist-util-visit";
import { lintRule } from "unified-lint-rule";
import semverParse from "semver/functions/parse.js";
import semverLt from "semver/functions/lt.js";

const allowedKeys = [
  "added",
  "napiVersion",
  "deprecated",
  "removed",
  "changes",
];
const changesExpectedKeys = ["version", "pr-url", "description"];
const VERSION_PLACEHOLDER = "REPLACEME";
const MAX_SAFE_SEMVER_VERSION = semverParse(
  Array.from({ length: 3 }, () => Number.MAX_SAFE_INTEGER).join(".")
);
const validVersionNumberRegex = /^v\d+\.\d+\.\d+$/;
const prUrlRegex = new RegExp("^https://github.com/nodejs/node/pull/\\d+$");
const privatePRUrl = "https://github.com/nodejs-private/node-private/pull/";

let releasedVersions;
let invalidVersionMessage = "version(s) must respect the pattern `vx.x.x` or";
if (process.env.NODE_RELEASED_VERSIONS) {
  console.log("Using release list from env...");
  releasedVersions = process.env.NODE_RELEASED_VERSIONS.split(",").map(
    (v) => `v${v}`
  );
  invalidVersionMessage = `version not listed in the changelogs, `;
}
invalidVersionMessage += `use the placeholder \`${VERSION_PLACEHOLDER}\``;

const kContainsIllegalKey = Symbol("illegal key");
const kWrongKeyOrder = Symbol("Wrong key order");
function unorderedKeys(meta) {
  const keys = Object.keys(meta);
  let previousKeyIndex = -1;
  for (const key of keys) {
    const keyIndex = allowedKeys.indexOf(key);
    if (keyIndex <= previousKeyIndex) {
      return keyIndex === -1 ? kContainsIllegalKey : kWrongKeyOrder;
    }
    previousKeyIndex = keyIndex;
  }
}

function containsInvalidVersionNumber(version) {
  if (Array.isArray(version)) {
    return version.some(containsInvalidVersionNumber);
  }

  if (version === undefined || version === VERSION_PLACEHOLDER) return false;

  if (
    releasedVersions &&
    // Always ignore 0.0.x and 0.1.x release numbers:
    (version[1] !== "0" || (version[3] !== "0" && version[3] !== "1"))
  )
    return !releasedVersions.includes(version);

  return !validVersionNumberRegex.test(version);
}
const getValidSemver = (version) =>
  version === VERSION_PLACEHOLDER ? MAX_SAFE_SEMVER_VERSION : version;
function areVersionsUnordered(versions) {
  if (!Array.isArray(versions)) return false;

  for (let index = 1; index < versions.length; index++) {
    if (
      semverLt(
        getValidSemver(versions[index - 1]),
        getValidSemver(versions[index])
      )
    ) {
      return true;
    }
  }
}

function invalidChangesKeys(change) {
  const keys = Object.keys(change);
  const { length } = keys;
  if (length !== changesExpectedKeys.length) return true;
  for (let index = 0; index < length; index++) {
    if (keys[index] !== changesExpectedKeys[index]) return true;
  }
}
function validateSecurityChange(file, node, change, index) {
  if ("commit" in change) {
    if (typeof change.commit !== "string" || isNaN(`0x${change.commit}`)) {
      file.message(
        `changes[${index}]: Ill-formed security change commit ID`,
        node
      );
    }

    if (Object.keys(change)[1] === "commit") {
      change = { ...change };
      delete change.commit;
    }
  }
  if (invalidChangesKeys(change)) {
    const securityChangeExpectedKeys = [...changesExpectedKeys];
    securityChangeExpectedKeys[0] += "[, commit]";
    file.message(
      `changes[${index}]: Invalid keys. Expected keys are: ` +
        securityChangeExpectedKeys.join(", "),
      node
    );
  }
}
function validateChanges(file, node, changes) {
  if (!Array.isArray(changes))
    return file.message("`changes` must be a YAML list", node);

  const changesVersions = [];
  for (let index = 0; index < changes.length; index++) {
    const change = changes[index];

    const isAncient =
      typeof change.version === "string" && change.version.startsWith("v0.");
    const isSecurityChange =
      !isAncient &&
      typeof change["pr-url"] === "string" &&
      change["pr-url"].startsWith(privatePRUrl);

    if (isSecurityChange) {
      validateSecurityChange(file, node, change, index);
    } else if (!isAncient && invalidChangesKeys(change)) {
      file.message(
        `changes[${index}]: Invalid keys. Expected keys are: ` +
          changesExpectedKeys.join(", "),
        node
      );
    }

    if (containsInvalidVersionNumber(change.version)) {
      file.message(`changes[${index}]: ${invalidVersionMessage}`, node);
    } else if (areVersionsUnordered(change.version)) {
      file.message(`changes[${index}]: list of versions is not in order`, node);
    }

    if (!isAncient && !isSecurityChange && !prUrlRegex.test(change["pr-url"])) {
      file.message(
        `changes[${index}]: PR-URL does not match the expected pattern`,
        node
      );
    }

    if (typeof change.description !== "string" || !change.description.length) {
      file.message(
        `changes[${index}]: must contain a non-empty description`,
        node
      );
    } else if (!change.description.endsWith(".")) {
      file.message(
        `changes[${index}]: description must end with a period`,
        node
      );
    }

    changesVersions.push(
      Array.isArray(change.version) ? change.version[0] : change.version
    );
  }

  if (areVersionsUnordered(changesVersions)) {
    file.message("Items in `changes` list are not in order", node);
  }
}

function validateMeta(node, file, meta) {
  switch (unorderedKeys(meta)) {
    case kContainsIllegalKey:
      file.message(
        "YAML dictionary contains illegal keys. Accepted values are: " +
          allowedKeys.join(", "),
        node
      );
      break;

    case kWrongKeyOrder:
      file.message(
        "YAML dictionary keys should be in this order: " +
          allowedKeys.join(", "),
        node
      );
      break;
  }

  if (containsInvalidVersionNumber(meta.added)) {
    file.message(`Invalid \`added\` value: ${invalidVersionMessage}`, node);
  } else if (areVersionsUnordered(meta.added)) {
    file.message("Versions in `added` list are not in order", node);
  }

  if (containsInvalidVersionNumber(meta.deprecated)) {
    file.message(
      `Invalid \`deprecated\` value: ${invalidVersionMessage}`,
      node
    );
  } else if (areVersionsUnordered(meta.deprecated)) {
    file.message("Versions in `deprecated` list are not in order", node);
  }

  if (containsInvalidVersionNumber(meta.removed)) {
    file.message(`Invalid \`removed\` value: ${invalidVersionMessage}`, node);
  } else if (areVersionsUnordered(meta.removed)) {
    file.message("Versions in `removed` list are not in order", node);
  }

  if ("changes" in meta) {
    validateChanges(file, node, meta.changes);
  }
}

function validateYAMLComments(tree, file) {
  visit(tree, "html", function visitor(node) {
    if (node.value.startsWith("<!--YAML\n"))
      file.message(
        "Expected `<!-- YAML`, found `<!--YAML`. Please add a space",
        node
      );
    if (!node.value.startsWith("<!-- YAML\n")) return;
    try {
      const meta = yaml.load("#" + node.value.slice(0, -"-->".length));

      validateMeta(node, file, meta);
    } catch (e) {
      file.message(e, node);
    }
  });
}

const remarkLintNodejsYamlComments = lintRule(
  "remark-lint:nodejs-yaml-comments",
  validateYAMLComments
);

export default remarkLintNodejsYamlComments;
