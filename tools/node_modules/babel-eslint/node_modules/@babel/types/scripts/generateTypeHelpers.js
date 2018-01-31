"use strict";
const fs = require("fs");
const path = require("path");
const generateBuilders = require("./generators/generateBuilders");
const generateValidators = require("./generators/generateValidators");
const generateAsserts = require("./generators/generateAsserts");
const generateConstants = require("./generators/generateConstants");
const format = require("./utils/formatCode");

const baseDir = path.join(__dirname, "../src");

function writeFile(content, location) {
  const file = path.join(baseDir, location);

  try {
    fs.mkdirSync(path.dirname(file));
  } catch (error) {
    if (error.code !== "EEXIST") {
      throw error;
    }
  }

  fs.writeFileSync(file, format(content, file));
}

console.log("Generating @babel/types dynamic functions");

writeFile(generateBuilders(), "builders/generated/index.js");
writeFile(generateValidators(), "validators/generated/index.js");
writeFile(generateAsserts(), "asserts/generated/index.js");
writeFile(generateConstants(), "constants/generated/index.js");
