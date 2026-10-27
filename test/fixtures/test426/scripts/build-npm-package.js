// Builds the contents for the @tc39/source-map-tests npm package,
// in the `npm-package` folder.

import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(import.meta.dirname, "..");
const outDir = path.join(root, "npm-package");
const committedFiles = new Set([
  "README.md",
  "package.template.json",
  "types",
]);

const errors = [];
const error = msg => errors.push(msg);

const rel = abs => path.relative(root, abs).split(path.sep).join("/");
const abs = relPath => path.join(root, relPath);
const out = relPath => path.join(outDir, relPath);

function walk(dir) {
  return fs
    .readdirSync(dir, { withFileTypes: true })
    .flatMap(e =>
      e.isDirectory() ? walk(path.join(dir, e.name)) : [path.join(dir, e.name)]
    );
}

/**
 * Returns `{ path, content }` for a repo-relative path, or null if missing.
 * `packagePath` is where the file ends up in the npm package.
 */
function fileEntry(relPath, context, packagePath = relPath) {
  if (!fs.existsSync(abs(relPath))) {
    error(`${context}: referenced file ${relPath} does not exist`);
    return null;
  }
  return { path: packagePath, content: fs.readFileSync(abs(relPath), "utf8") };
}

function checkUniqueNames(suiteName, tests) {
  const seen = new Set();
  for (const { name } of tests) {
    if (seen.has(name)) error(`${suiteName}: duplicate test name ${name}`);
    seen.add(name);
  }
}

// Interpreting suites: <name>[-proposal]-tests.json manifests in the repo root,
// whose files live in resources/<resourceBasePath>. In the package, resources/
// is copied to interpreting/resources/.

const referencedResources = new Set();

function buildInterpretingSuite(manifestFile) {
  const name = manifestFile.replace(/-tests\.json$/, "").replace(/-proposal$/, "");
  const manifest = JSON.parse(fs.readFileSync(abs(manifestFile), "utf8"));
  const base = path.posix.join("resources", manifest.resourceBasePath ?? "");
  const resource = (file, context) => {
    const relPath = path.posix.join(base, file);
    referencedResources.add(relPath);
    return fileEntry(relPath, context, `interpreting/${relPath}`);
  };

  const tests = manifest.tests.map(test => {
    const context = `${manifestFile} (${test.name})`;
    return {
      name: test.name,
      description: test.description,
      sourceMapIsValid: test.sourceMapIsValid,
      baseFile: resource(test.baseFile, context),
      sourceMapFile: resource(test.sourceMapFile, context),
      testActions: (test.testActions ?? []).map(action =>
        action.intermediateMaps
          ? {
              ...action,
              intermediateMaps: action.intermediateMaps.map(m =>
                resource(m, context)
              ),
            }
          : action
      ),
    };
  });
  checkUniqueNames(name, tests);
  return { name, kind: "interpreting", tests };
}

// Decoding suites: each directory under decoding/ containing <name>.map files,
// each with a <name>.map.golden and an optional <name>.error/<name>.map.error.

function buildDecodingSuite(dir) {
  const name = path.basename(dir);
  const files = fs.readdirSync(dir).sort();
  const used = new Set();
  const tests = [];

  for (const file of files.filter(f => f.endsWith(".map"))) {
    const testName = file.slice(0, -".map".length);
    const context = `${rel(dir)} (${testName})`;
    const sourceMapFile = fileEntry(rel(path.join(dir, file)), context);
    const goldenFile = fileEntry(rel(path.join(dir, `${file}.golden`)), context);
    const errorFile = [`${testName}.error`, `${file}.error`].find(f =>
      files.includes(f)
    );
    used.add(file).add(`${file}.golden`).add(errorFile);

    if (goldenFile) {
      try {
        JSON.parse(goldenFile.content);
      } catch (e) {
        error(`${goldenFile.path}: invalid JSON (${e.message})`);
      }
    }
    tests.push({
      name: testName,
      sourceMapFile,
      goldenFile,
      optionalErrors: errorFile !== undefined,
    });
  }

  for (const file of files) {
    if (/\.(golden|error)$/.test(file) && !used.has(file)) {
      error(`${rel(path.join(dir, file))}: no matching .map file`);
    }
  }

  checkUniqueNames(name, tests);
  return { name, kind: "decoding", tests };
}

// Stable spec tests come first, followed by proposals.
const isProposal = f => f.endsWith("-proposal-tests.json");
const interpretingSuites = fs
  .readdirSync(root)
  .filter(f => f.endsWith("-tests.json"))
  .sort((a, b) => isProposal(a) - isProposal(b) || a.localeCompare(b))
  .map(buildInterpretingSuite);

const decodingDirs = [
  path.join(root, "decoding"),
  ...fs
    .readdirSync(path.join(root, "decoding"), { withFileTypes: true })
    .filter(e => e.isDirectory())
    .map(e => path.join(root, "decoding", e.name))
    .sort(),
];
const decodingSuites = decodingDirs
  .filter(dir => fs.readdirSync(dir).some(f => f.endsWith(".map")))
  .map(buildDecodingSuite);

const suites = [...interpretingSuites, ...decodingSuites];

// Suites go in <kind>/<name>.json, next to the directory with their files.
const suiteFile = ({ name, kind }) => `${kind}/${name}.json`;

const suiteFiles = new Set();
for (const suite of suites) {
  const file = suiteFile(suite);
  if (suiteFiles.has(file)) error(`two suites would be written to ${file}`);
  suiteFiles.add(file);
}

for (const file of walk(path.join(root, "resources"))) {
  if (file.endsWith(".map") && !referencedResources.has(rel(file))) {
    error(`${rel(file)}: source map not referenced by any test`);
  }
}

if (errors.length > 0) {
  console.error(`Found ${errors.length} problem(s) in the test data:`);
  for (const msg of errors) console.error(`  - ${msg}`);
  process.exit(1);
}

// Write the package.

const writeJSON = (relPath, data) => {
  fs.mkdirSync(path.dirname(out(relPath)), { recursive: true });
  fs.writeFileSync(out(relPath), JSON.stringify(data, null, 2) + "\n");
};

const git = (...gitArgs) =>
  execFileSync("git", gitArgs, { cwd: root, encoding: "utf8" }).trim();
const version = `0.0.${git("rev-list", "--count", "HEAD")}`;
const gitHead = git("rev-parse", "HEAD");

for (const entry of fs.readdirSync(outDir)) {
  if (!committedFiles.has(entry)) {
    fs.rmSync(out(entry), { recursive: true, force: true });
  }
}

fs.cpSync(abs("resources"), out("interpreting/resources"), { recursive: true });
fs.cpSync(abs("decoding"), out("decoding"), { recursive: true });
fs.copyFileSync(abs("LICENSE.md"), out("LICENSE.md"));

for (const suite of suites) {
  writeJSON(suiteFile(suite), { tests: suite.tests });
}
const packageJson = JSON.parse(
  fs.readFileSync(out("package.template.json"), "utf8")
);

const decodingTypes = walk(abs("decoding"))
  .filter(f => f.endsWith(".d.ts"))
  .map(rel)
  .sort();
const exportedFiles = [
  "package.json",
  "suites.json",
  ...suites.map(suiteFile),
  ...decodingTypes,
];
writeJSON("package.json", {
  ...packageJson,
  version,
  gitHead,
  exports: {
    ".": {
      "types@>=7.1.0-0": "./types/index.d.ts",
      types: "./types/types.d.ts",
    },
    ...Object.fromEntries(exportedFiles.map(f => [`./${f}`, `./${f}`])),
  },
});
writeJSON("suites.json", {
  suites: suites.map(suite => ({
    kind: suite.kind,
    file: suiteFile(suite),
  })),
});

console.log(`Built ${packageJson.name}@${version} in ${rel(outDir)}/`);
for (const { name, kind, tests } of suites) {
  console.log(`  ${name} (${kind}): ${tests.length} tests`);
}
