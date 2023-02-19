"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.getTrustedRoot = void 0;
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
const path_1 = __importDefault(require("path"));
const tuf_js_1 = require("tuf-js");
const trustroot_1 = require("./trustroot");
async function getTrustedRoot(cacheDir) {
    initTufCache(cacheDir);
    const repoMap = initRepoMap(cacheDir);
    const repoClients = Object.entries(repoMap.repositories).map(([name, urls]) => initClient(name, urls[0], cacheDir));
    // TODO: Add support for multiple repositories. For now, we just use the first
    // one (the production Sigstore TUF repository).
    const fetcher = new trustroot_1.TrustedRootFetcher(repoClients[0]);
    return fetcher.getTrustedRoot();
}
exports.getTrustedRoot = getTrustedRoot;
// Initializes the root TUF cache directory
function initTufCache(cacheDir) {
    if (!fs_1.default.existsSync(cacheDir)) {
        fs_1.default.mkdirSync(cacheDir, { recursive: true });
    }
}
// Initializes the repo map (copying it to the cache root dir) and returns the
// content of the repository map.
function initRepoMap(rootDir) {
    const mapDest = path_1.default.join(rootDir, 'map.json');
    if (!fs_1.default.existsSync(mapDest)) {
        const mapSrc = require.resolve('../../store/map.json');
        fs_1.default.copyFileSync(mapSrc, mapDest);
    }
    const buf = fs_1.default.readFileSync(mapDest);
    return JSON.parse(buf.toString('utf-8'));
}
function initClient(name, url, rootDir) {
    const repoCachePath = path_1.default.join(rootDir, name);
    const targetCachePath = path_1.default.join(repoCachePath, 'targets');
    const tufRootDest = path_1.default.join(repoCachePath, 'root.json');
    // Only copy the TUF trusted root if it doesn't already exist. It's possible
    // that the cached root has already been updated, so we don't want to roll it
    // back.
    if (!fs_1.default.existsSync(tufRootDest)) {
        const tufRootSrc = require.resolve(`../../store/${name}-root.json`);
        fs_1.default.mkdirSync(repoCachePath);
        fs_1.default.copyFileSync(tufRootSrc, tufRootDest);
    }
    if (!fs_1.default.existsSync(targetCachePath)) {
        fs_1.default.mkdirSync(targetCachePath);
    }
    // TODO: Is there some better way to derive the base URL for the targets?
    // Hard-coding for now based on current Sigstore TUF repo layout.
    return new tuf_js_1.Updater({
        metadataBaseUrl: url,
        targetBaseUrl: `${url}/targets`,
        metadataDir: repoCachePath,
        targetDir: targetCachePath,
    });
}
