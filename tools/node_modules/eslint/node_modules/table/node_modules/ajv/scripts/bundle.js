"use strict"

const fs = require("fs")
const path = require("path")
const browserify = require("browserify")
const {minify} = require("terser")

const [sourceFile, outFile, globalName] = process.argv.slice(2)

const json = require(path.join(__dirname, "..", "package.json"))
const bundleDir = path.join(__dirname, "..", "bundle")
if (!fs.existsSync(bundleDir)) fs.mkdirSync(bundleDir)

browserify({standalone: globalName})
  .require(path.join(__dirname, "../dist", sourceFile), {expose: sourceFile})
  .bundle(saveAndMinify)

async function saveAndMinify(err, buf) {
  if (err) {
    console.error("browserify error:", err)
    process.exit(1)
  }

  const bundlePath = path.join(bundleDir, outFile)
  const opts = {
    ecma: 2018,
    warnings: true,
    compress: {
      pure_getters: true,
      keep_infinity: true,
      unsafe_methods: true,
    },
    format: {
      preamble: `/* ${json.name} ${json.version} (${globalName}): ${json.description} */`,
    },
    sourceMap: {
      filename: outFile + ".min.js",
      url: outFile + ".min.js.map",
    },
  }

  const result = await minify(buf.toString(), opts)

  fs.writeFileSync(bundlePath + ".bundle.js", buf)
  fs.writeFileSync(bundlePath + ".min.js", result.code)
  fs.writeFileSync(bundlePath + ".min.js.map", result.map)
  if (result.warnings) result.warnings.forEach((msg) => console.warn("terser.minify warning:", msg))
}
