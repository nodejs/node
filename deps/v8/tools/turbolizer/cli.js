#!/usr/bin/env node
// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { spawn } from "node:child_process";
import { readFile, stat } from "node:fs/promises";
import { createServer } from "node:http";
import { basename, resolve } from "node:path";
import { parseArgs } from "node:util";

const TURBOLIZER_URL = "https://v8.github.io/tools/head/turbolizer";

function usage() {
  console.error(
    "Usage: turbolizer [-n|--no-open] [--url URL] [--port PORT] " +
    "path/to/turbo.json [path/to/another.json ...]");
}

async function parseCommandLineArgs() {
  const { values, positionals } = parseArgs({
    options: {
      help: {
        type: "boolean",
        short: "h"
      },
      "no-open": {
        type: "boolean",
        short: "n"
      },
      url: {
        type: "string"
      },
      port: {
        type: "string"
      }
    },
    allowPositionals: true
  });

  if (values.help) {
    usage();
    process.exit(0);
  }

  if (positionals.length === 0) {
    usage();
    process.exit(1);
  }

  const jsonFiles = [];
  for (const arg of positionals) {
    const path = resolve(arg);
    if (!(await stat(path)).isFile()) {
      throw new Error(`${arg} is not a file`);
    }
    jsonFiles.push(path);
  }

  return {
    jsonFiles,
    url: new URL(values.url || TURBOLIZER_URL),
    openBrowser: !values["no-open"],
    // By default, use 0 to listen on any localhost port available.
    port: Number(values.port ?? 0)
  };
}

function open(url) {
  let command;
  switch (process.platform) {
    case "darwin":
      command = "open";
      break;
    case "linux":
      command = "xdg-open";
      break;
    default:
      throw new Error(`Failed to open browser on ${process.platform}`);
  }
  const child = spawn(command, [url], {
    detached: true,
    stdio: "ignore"
  });
  child.on("error", err => {
    console.error(`Failed to open browser: ${err.message}`);
  });
  child.unref();
}

async function main() {
  const { jsonFiles, url, openBrowser, port } =
    await parseCommandLineArgs();

  for (const path of jsonFiles) {
    const data = await readFile(path);
    // A "one-shot" server that exits after the file is served.
    const server = createServer((request, response) => {
      response.writeHead(200, {
        "access-control-allow-origin": url.origin,
        "content-type": "application/json; charset=utf-8"
      });
      response.end(data, () => server.close());
    });

    server.listen(port, "localhost", () => {
      const fileUrl = new URL(url);
      const { port } = server.address();
      fileUrl.searchParams.set("url", `http://localhost:${port}`);

      if (openBrowser) {
        console.log(`Opening Turbolizer for ${basename(path)}...`);
        open(fileUrl);
      } else {
        console.log(`Turbolizer ready on ${fileUrl.toString()}.`);
      }

      console.log(`Serving ${basename(path)}.`);
    });

    server.on("error", err => { throw err; });
  }
}

main().catch(err => {
  console.error(err.message || err);
  process.exit(1);
});
