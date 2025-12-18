// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const util = require('util');
const execFile = util.promisify(require('child_process').execFile);
const fs = require('fs')

async function sh(cmd, ...params) {
  console.log(cmd, params.join(' '));
  const options = {maxBuffer: 256 * 1024 * 1024};
  const {stdout} = await execFile(cmd, params, options);
  return stdout;
}

class Symbolizer {
  constructor() {
    this.nmExec = 'nm';
    this.objdumpExec = 'objdump';
  }

  optionDefinitions() {
    return [
      {
        name: 'apk-embedded-library',
        type: String,
        description:
            'Specify the path of the embedded library for Android traces',
      },
      {
        name: 'target',
        type: String,
        description: 'Specify the target root directory for cross environment',
      }
    ]
  }

  middleware(config) {
    return async (ctx, next) => {
      if (ctx.path == '/v8/loadVMSymbols') {
        await this.parseVMSymbols(ctx)
      } else if (ctx.path == '/v8/info/platform') {
        ctx.response.type = 'json';
        ctx.response.body = JSON.stringify({
          'name': process.platform,
          'nmExec': this.nmExec,
          'objdumpExec': this.objdumpExec,
          'targetRootFS': config.target,
          'apkEmbeddedLibrary': config.apkEmbeddedLibrary
        });
      }
      await next();
    }
  }

  async parseVMSymbols(ctx) {
    const query = ctx.request.query;
    const result = {
      libName: query.libName,
      symbols: ['', ''],
      error: undefined,
      fileOffsetMinusVma: 0,
    };
    switch (query.platform) {
      case 'macos':
        await this.loadVMSymbolsMacOS(query, result);
        break;
      case 'linux':
        await this.loadVMSymbolsLinux(query, result);
        break;
      default:
        ctx.response.status = '500';
        return;
    }
    ctx.response.type = 'json';
    ctx.response.body = JSON.stringify(result);
  }

  async loadVMSymbolsMacOS(query, result) {
    let libName =
        (query.targetRootFS ? query.targetRootFS : '') + query.libName;
    try {
      // Fast skip files that don't exist.
      if (libName.indexOf('/') === -1 || !fs.existsSync(libName)) return;
      result.symbols = [await sh(this.nmExec, '--demangle', '-n', libName), ''];
    } catch (e) {
      result.error = e.message;
    }
  }

  async loadVMSymbolsLinux(query, result) {
    let libName = query.libName;
    if (query.apkEmbeddedLibrary && libName.endsWith('.apk')) {
      libName = query.apkEmbeddedLibrary;
    }
    if (query.targetRootFS) {
      libName = libName.substring(libName.lastIndexOf('/') + 1);
      libName = query.targetRootFS + libName;
    }
    try {
      // Fast skip files that don't exist.
      if (libName.indexOf('/') === -1 || !fs.existsSync(libName)) return;
      result.symbols = [
        await sh(this.nmExec, '-C', '-n', '-S', libName),
        await sh(this.nmExec, '-C', '-n', '-S', '-D', libName)
      ];

      const objdumpOutput = await sh(this.objdumpExec, '-h', libName);
      for (const line of objdumpOutput.split('\n')) {
        const [, sectionName, , vma, , fileOffset] = line.trim().split(/\s+/);
        if (sectionName === '.text') {
          result.fileOffsetMinusVma =
              parseInt(fileOffset, 16) - parseInt(vma, 16);
        }
      }
    } catch (e) {
      console.log(e);
      result.error = e.message;
    }
  }
}

module.exports = Symbolizer
