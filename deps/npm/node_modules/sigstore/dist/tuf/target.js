"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.getTarget = void 0;
/*
Copyright 2023 The Sigstore Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
const fs_1 = __importDefault(require("fs"));
const error_1 = require("../error");
// Returns the local path to the specified target. If the target is not yet
// cached locally, the provided TUF Updater will be used to download and
// cache the target.
async function getTarget(tuf, targetPath) {
    const path = await getTargetPath(tuf, targetPath);
    try {
        return fs_1.default.readFileSync(path, 'utf-8');
    }
    catch (err) {
        throw new error_1.InternalError(`error reading trusted root: ${err}`);
    }
}
exports.getTarget = getTarget;
async function getTargetPath(tuf, target) {
    let targetInfo;
    try {
        targetInfo = await tuf.refresh().then(() => tuf.getTargetInfo(target));
    }
    catch (err) {
        throw new error_1.InternalError(`error refreshing TUF metadata: ${err}`);
    }
    if (!targetInfo) {
        throw new error_1.InternalError(`target ${target} not found`);
    }
    let path = await tuf.findCachedTarget(targetInfo);
    // An empty path here means the target has not been cached locally, or is
    // out of date. In either case, we need to download it.
    if (!path) {
        try {
            path = await tuf.downloadTarget(targetInfo);
        }
        catch (err) {
            throw new error_1.InternalError(`error downloading target: ${err}`);
        }
    }
    return path;
}
