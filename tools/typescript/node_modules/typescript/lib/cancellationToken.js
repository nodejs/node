/*! *****************************************************************************
Copyright (c) Microsoft Corporation. All rights reserved.
Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at http://www.apache.org/licenses/LICENSE-2.0

THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
MERCHANTABLITY OR NON-INFRINGEMENT.

See the Apache Version 2.0 License for specific language governing permissions
and limitations under the License.
***************************************************************************** */


"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));

// src/cancellationToken/cancellationToken.ts
var fs = __toESM(require("fs"));
function pipeExists(name) {
  return fs.existsSync(name);
}
function createCancellationToken(args) {
  let cancellationPipeName;
  for (let i = 0; i < args.length - 1; i++) {
    if (args[i] === "--cancellationPipeName") {
      cancellationPipeName = args[i + 1];
      break;
    }
  }
  if (!cancellationPipeName) {
    return {
      isCancellationRequested: () => false,
      setRequest: (_requestId) => void 0,
      resetRequest: (_requestId) => void 0
    };
  }
  if (cancellationPipeName.charAt(cancellationPipeName.length - 1) === "*") {
    const namePrefix = cancellationPipeName.slice(0, -1);
    if (namePrefix.length === 0 || namePrefix.includes("*")) {
      throw new Error("Invalid name for template cancellation pipe: it should have length greater than 2 characters and contain only one '*'.");
    }
    let perRequestPipeName;
    let currentRequestId;
    return {
      isCancellationRequested: () => perRequestPipeName !== void 0 && pipeExists(perRequestPipeName),
      setRequest(requestId) {
        currentRequestId = requestId;
        perRequestPipeName = namePrefix + requestId;
      },
      resetRequest(requestId) {
        if (currentRequestId !== requestId) {
          throw new Error(`Mismatched request id, expected ${currentRequestId}, actual ${requestId}`);
        }
        perRequestPipeName = void 0;
      }
    };
  } else {
    return {
      isCancellationRequested: () => pipeExists(cancellationPipeName),
      setRequest: (_requestId) => void 0,
      resetRequest: (_requestId) => void 0
    };
  }
}
module.exports = createCancellationToken;
//# sourceMappingURL=cancellationToken.js.map
