// This file is to be concatenated with
// https://github.com/microsoft/TypeScript/blob/main/lib/typescript.js
// to produce a snapshot that reads a file path from the command
// line and compile it into JavaScript, then write the
// result into another file.

const fs = require('fs');
const v8 = require('v8');
const assert = require('assert');

v8.startupSnapshot.setDeserializeMainFunction(( { ts }) => {
  const input = process.argv[1];
  const output = process.argv[2];
  console.error(`Compiling ${input} to ${output}`);
  assert(input);
  assert(output);
  const source = fs.readFileSync(input, 'utf8');

  let result = ts.transpileModule(
    source,
    {
      compilerOptions: {
        module: ts.ModuleKind.CommonJS
      }
    });

  fs.writeFileSync(output, result.outputText, 'utf8');
}, { ts });
