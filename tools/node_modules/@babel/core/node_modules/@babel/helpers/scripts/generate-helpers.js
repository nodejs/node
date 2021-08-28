import fs from "fs";
import { join } from "path";
import { URL, fileURLToPath } from "url";

const HELPERS_FOLDER = new URL("../src/helpers", import.meta.url);
const IGNORED_FILES = new Set(["package.json"]);

export default async function generateAsserts() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */

import template from "@babel/template";

`;

  for (const file of (await fs.promises.readdir(HELPERS_FOLDER)).sort()) {
    if (IGNORED_FILES.has(file)) continue;
    if (file.startsWith(".")) continue; // ignore e.g. vim swap files

    const [helperName] = file.split(".");
    const isValidId = isValidBindingIdentifier(helperName);
    const varName = isValidId ? helperName : `_${helperName}`;

    const filePath = join(fileURLToPath(HELPERS_FOLDER), file);
    const fileContents = await fs.promises.readFile(filePath, "utf8");
    const minVersionMatch = fileContents.match(
      /^\s*\/\*\s*@minVersion\s+(?<minVersion>\S+)\s*\*\/\s*$/m
    );
    if (!minVersionMatch) {
      throw new Error(`@minVersion number missing in ${filePath}`);
    }
    const { minVersion } = minVersionMatch.groups;

    // TODO: We can minify the helpers in production
    const source = fileContents
      // Remove comments
      .replace(/\/\*[^]*?\*\/|\/\/.*/g, "")
      // Remove multiple newlines
      .replace(/\n{2,}/g, "\n");

    const intro = isValidId
      ? "export "
      : `export { ${varName} as ${helperName} }\n`;

    output += `\n${intro}const ${varName} = {
  minVersion: ${JSON.stringify(minVersion)},
  ast: () => template.program.ast(${JSON.stringify(source)})
};\n`;
  }

  return output;
}

function isValidBindingIdentifier(name) {
  try {
    Function(`var ${name}`);
    return true;
  } catch {
    return false;
  }
}
