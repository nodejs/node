// Checks that the JSON files in the generated npm package match
// the TS definitions. Run build-npm-package.js first.

import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(import.meta.dirname, "..");
const outDir = path.join(root, ".tmp", "check-types");
const tsc = path.join(root, "node_modules", ".bin", "tsc");

const compilerOptions = {
  strict: true,
  noEmit: true,
  skipLibCheck: false,
  module: "nodenext",
  moduleResolution: "nodenext",
  target: "esnext",
  types: [],
};

const relFrom = (from, to) => {
  const rel = path.relative(path.dirname(from), to).split(path.sep).join("/");
  return rel.startsWith(".") ? rel : `./${rel}`;
};

function writeCheck(tsFile, header, type, jsonFile) {
  const json = fs.readFileSync(jsonFile, "utf8");
  fs.mkdirSync(path.dirname(tsFile), { recursive: true });
  fs.writeFileSync(
    tsFile,
    `${header}\nexport default ${json.trim()} satisfies ${type};\n`
  );
}

function runTsc(name, projectDir, files) {
  const tsconfig = path.join(projectDir, "tsconfig.json");
  fs.writeFileSync(tsconfig, JSON.stringify({ compilerOptions, files }, null, 2));
  const { status } = spawnSync(tsc, ["-p", tsconfig, "--pretty"], {
    stdio: "inherit",
  });
  console.log(`${status === 0 ? "ok" : "FAIL"} ${name}`);
  return status === 0;
}

fs.rmSync(outDir, { recursive: true, force: true });
let ok = true;

// npm package files.

const pkgDir = path.join(root, "npm-package");
if (!fs.existsSync(path.join(pkgDir, "suites.json"))) {
  console.error("npm-package/suites.json not found, run scripts/build-npm-package.js first.");
  process.exit(1);
}
{
  const projectDir = path.join(outDir, "npm-package");
  const typesFile = path.join(pkgDir, "types", "types.d.ts");
  const files = [];
  const check = (jsonRel, type) => {
    const tsFile = path.join(projectDir, `${jsonRel}.ts`);
    const header = `import type { ${type} } from "${relFrom(tsFile, typesFile)}";`;
    writeCheck(tsFile, header, type, path.join(pkgDir, jsonRel));
    files.push(path.relative(projectDir, tsFile));
  };

  const index = JSON.parse(fs.readFileSync(path.join(pkgDir, "suites.json"), "utf8"));
  const expectedTypes = new Map([["suites.json", "SuitesIndex"]]);
  for (const { kind, file } of index.suites) {
    expectedTypes.set(file, kind === "interpreting" ? "InterpretingSuite" : "DecodingSuite");
  }
  for (const [file, type] of expectedTypes) check(file, type);

  // Check that importing each exported JSON file `with { type: "json" }` gets
  // its declared type from modules.d.ts.
  const { name: pkgName, exports } = JSON.parse(
    fs.readFileSync(path.join(pkgDir, "package.json"), "utf8")
  );
  const exportedJSON = Object.keys(exports)
    .filter(key => key.endsWith(".json") && key !== "./package.json")
    .map(key => key.slice(2));
  const importsFile = path.join(projectDir, "imports.ts");
  const typeNames = [...new Set(expectedTypes.values())];
  fs.writeFileSync(
    importsFile,
    [
      `import type { ${typeNames.join(", ")} } from "${relFrom(importsFile, path.join(pkgDir, "types", "index.js"))}";`,
      "type Equals<A, B> = (<T>() => T extends A ? 1 : 2) extends (<T>() => T extends B ? 1 : 2) ? true : false;",
      ...exportedJSON.flatMap((file, i) => {
        const type = expectedTypes.get(file);
        if (!type) throw new Error(`Exported ${file} is not a known suite file`);
        return [
          `import json${i} from "${pkgName}/${file}" with { type: "json" };`,
          `export const check${i}: Equals<typeof json${i}, ${type}> = true; // ${file}`,
        ];
      }),
    ].join("\n") + "\n"
  );
  files.push(path.relative(projectDir, importsFile));

  ok = runTsc("npm-package", projectDir, files) && ok;

  // Check that each exported decoding/**/*.d.ts file can be loaded through the
  // package's exports. Each one is checked separately, because they extend
  // the same global interfaces.
  for (const key of Object.keys(exports).filter(k => k.endsWith(".d.ts"))) {
    const name = `npm-package ${key.slice(2)}`;
    const dtsDir = path.join(outDir, "exports", key.slice(2));
    const linkDir = path.join(dtsDir, "node_modules", ...pkgName.split("/"));
    fs.mkdirSync(path.dirname(linkDir), { recursive: true });
    fs.symlinkSync(pkgDir, linkDir, "dir");
    fs.writeFileSync(
      path.join(dtsDir, "index.ts"),
      `/// <reference types="${pkgName}/${key.slice(2)}" />\nexport type T = SourceMapRecord;\n`
    );
    ok = runTsc(name, dtsDir, ["index.ts"]) && ok;
  }
}

// Check golden files

const decodingDir = path.join(root, "decoding");
const decodingSuiteDirs = [
  decodingDir,
  ...fs
    .readdirSync(decodingDir, { withFileTypes: true })
    .filter(e => e.isDirectory())
    .map(e => path.join(decodingDir, e.name)),
];
for (const dir of decodingSuiteDirs) {
  const goldens = fs.readdirSync(dir).filter(f => f.endsWith(".golden")).sort();
  if (goldens.length === 0) continue;

  const name = path.relative(root, dir).split(path.sep).join("/");
  const projectDir = path.join(outDir, name);
  const suiteTypes = path.join(dir, `${path.basename(dir)}.d.ts`);
  const typesFile = fs.existsSync(suiteTypes)
    ? suiteTypes
    : path.join(decodingDir, "source-map-record.d.ts");

  const files = goldens.map(golden => {
    const tsFile = path.join(projectDir, `${golden}.ts`);
    const header = `/// <reference path="${relFrom(tsFile, typesFile)}" />`;
    writeCheck(tsFile, header, "SourceMapRecord", path.join(dir, golden));
    return path.relative(projectDir, tsFile);
  });
  ok = runTsc(name, projectDir, files) && ok;
}

process.exit(ok ? 0 : 1);
