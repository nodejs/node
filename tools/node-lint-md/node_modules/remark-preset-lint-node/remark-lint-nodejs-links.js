import fs from "fs";
import path from "path";
import { pathToFileURL } from "url";

import { lintRule } from "unified-lint-rule";

function* getLinksRecursively(node) {
  if (node.url) {
    yield node;
  }
  for (const child of node.children || []) {
    yield* getLinksRecursively(child);
  }
}

function validateLinks(tree, vfile) {
  const currentFileURL = pathToFileURL(path.join(vfile.cwd, vfile.path));
  let previousDefinitionLabel;
  for (const node of getLinksRecursively(tree)) {
    if (node.url[0] !== "#") {
      const targetURL = new URL(node.url, currentFileURL);
      if (targetURL.protocol === "file:" && !fs.existsSync(targetURL)) {
        vfile.message("Broken link", node);
      } else if (targetURL.pathname === currentFileURL.pathname) {
        const expected = node.url.includes("#")
          ? node.url.slice(node.url.indexOf("#"))
          : "#";
        vfile.message(
          `Self-reference must start with hash (expected "${expected}", got "${node.url}")`,
          node
        );
      }
    }
    if (node.type === "definition") {
      if (previousDefinitionLabel && previousDefinitionLabel > node.label) {
        vfile.message(
          `Unordered reference ("${node.label}" should be before "${previousDefinitionLabel}")`,
          node
        );
      }
      previousDefinitionLabel = node.label;
    }
  }
}

const remarkLintNodejsLinks = lintRule(
  "remark-lint:nodejs-links",
  validateLinks
);

export default remarkLintNodejsLinks;
