import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

export async function load(url, context, nextLoad) {
  const result = await nextLoad(url, context);
  if (url.endsWith('/common/index.js')) {
    result.source = '"use strict";module.exports=require("node:module").createRequire(' +
             JSON.stringify(url) + ')(' + JSON.stringify(fileURLToPath(url)) + ');\n';
  } else if (url.startsWith('file:') && (context.format == null || context.format === 'commonjs')) {
    result.source = await readFile(new URL(url));
  }
  return result;
}
