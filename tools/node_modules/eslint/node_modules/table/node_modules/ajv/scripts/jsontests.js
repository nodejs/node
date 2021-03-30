"use strict"

const testSuitePaths = {
  draft6: "spec/JSON-Schema-Test-Suite/tests/draft6/",
  draft7: "spec/JSON-Schema-Test-Suite/tests/draft7/",
  draft2019: "spec/JSON-Schema-Test-Suite/tests/draft2019-09/",
  draft2020: "spec/JSON-Schema-Test-Suite/tests/draft2020-12/",
  tests: "spec/tests/",
  security: "spec/security/",
  extras: "spec/extras/",
  async: "spec/async/",
}

const glob = require("glob")
const fs = require("fs")

for (const suite in testSuitePaths) {
  const p = testSuitePaths[suite]
  const files = glob.sync(`${p}{**/,}*.json`)
  if (files.length === 0) {
    console.error(`Missing folder ${p}\nTry: git submodule update --init\n`)
    process.exit(1)
  }
  const code = files
    .map((f) => {
      const name = f.replace(p, "").replace(/\.json$/, "")
      const testPath = f.replace(/^spec/, "..")
      return `\n  {name: "${name}", test: require("${testPath}")},`
    })
    .reduce((list, f) => list + f)
  fs.writeFileSync(`./spec/_json/${suite}.js`, `module.exports = [${code}\n]\n`)
}
