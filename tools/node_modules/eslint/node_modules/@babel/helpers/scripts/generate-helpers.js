import fs from "fs";
import { join } from "path";
import { URL, fileURLToPath } from "url";
import { minify } from "terser"; // eslint-disable-line import/no-extraneous-dependencies

const HELPERS_FOLDER = new URL("../src/helpers", import.meta.url);
const IGNORED_FILES = new Set(["package.json"]);

export default async function generateHelpers() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'yarn gulp generate-runtime-helpers'
 */

import template from "@babel/template";

function helper(minVersion: string, source: string) {
  return Object.freeze({
    minVersion,
    ast: () => template.program.ast(source, { preserveComments: true }),
  })
}

export default Object.freeze({
`;

  for (const file of (await fs.promises.readdir(HELPERS_FOLDER)).sort()) {
    if (IGNORED_FILES.has(file)) continue;
    if (file.startsWith(".")) continue; // ignore e.g. vim swap files

    const [helperName] = file.split(".");

    const filePath = join(fileURLToPath(HELPERS_FOLDER), file);
    if (!file.endsWith(".js")) {
      console.error("ignoring", filePath);
      continue;
    }

    const fileContents = await fs.promises.readFile(filePath, "utf8");
    const minVersionMatch = fileContents.match(
      /^\s*\/\*\s*@minVersion\s+(?<minVersion>\S+)\s*\*\/\s*$/m
    );
    if (!minVersionMatch) {
      throw new Error(`@minVersion number missing in ${filePath}`);
    }
    const { minVersion } = minVersionMatch.groups;

    const source = await minify(fileContents, {
      mangle: false,
      // The _typeof helper has a custom directive that we must keep
      compress: { directives: false },
    });

    output += `\
  ${JSON.stringify(helperName)}: helper(
    ${JSON.stringify(minVersion)},
    ${JSON.stringify(source.code)},
  ),
`;
  }

  output += "});";
  return output;
}
