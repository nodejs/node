"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.TUFClient = void 0;
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
const target_1 = require("./target");
class TUFClient {
    constructor(options) {
        initTufCache(options.cachePath, options.rootPath);
        const remote = initRemoteConfig(options.cachePath, options.mirrorURL);
        this.updater = initClient(options.cachePath, remote, options);
    }
    async refresh() {
        return this.updater.refresh();
    }
    getTarget(targetName) {
        return (0, target_1.readTarget)(this.updater, targetName);
    }
}
exports.TUFClient = TUFClient;
// Initializes the TUF cache directory structure including the initial
// root.json file. If the cache directory does not exist, it will be
// created. If the targets directory does not exist, it will be created.
// If the root.json file does not exist, it will be copied from the
// rootPath argument.
function initTufCache(cachePath, tufRootPath) {
    const targetsPath = path_1.default.join(cachePath, 'targets');
    const cachedRootPath = path_1.default.join(cachePath, 'root.json');
    if (!fs_1.default.existsSync(cachePath)) {
        fs_1.default.mkdirSync(cachePath, { recursive: true });
    }
    if (!fs_1.default.existsSync(targetsPath)) {
        fs_1.default.mkdirSync(targetsPath);
    }
    if (!fs_1.default.existsSync(cachedRootPath)) {
        fs_1.default.copyFileSync(tufRootPath, cachedRootPath);
    }
    return cachePath;
}
// Initializes the remote.json file, which contains the URL of the TUF
// repository. If the file does not exist, it will be created. If the file
// exists, it will be parsed and returned.
function initRemoteConfig(rootDir, mirrorURL) {
    let remoteConfig;
    const remoteConfigPath = path_1.default.join(rootDir, 'remote.json');
    if (fs_1.default.existsSync(remoteConfigPath)) {
        const data = fs_1.default.readFileSync(remoteConfigPath, 'utf-8');
        remoteConfig = JSON.parse(data);
    }
    if (!remoteConfig) {
        remoteConfig = { mirror: mirrorURL };
        fs_1.default.writeFileSync(remoteConfigPath, JSON.stringify(remoteConfig));
    }
    return remoteConfig;
}
function initClient(cachePath, remote, options) {
    const baseURL = remote.mirror;
    const config = {
        fetchTimeout: options.timeout,
    };
    // tuf-js only supports a number for fetchRetries so we have to
    // convert the boolean and object options to a number.
    /* istanbul ignore if */
    if (typeof options.retry !== 'undefined') {
        if (typeof options.retry === 'number') {
            config.fetchRetries = options.retry;
        }
        else if (typeof options.retry === 'object') {
            config.fetchRetries = options.retry.retries;
        }
        else if (options.retry === true) {
            config.fetchRetries = 1;
        }
    }
    return new tuf_js_1.Updater({
        metadataBaseUrl: baseURL,
        targetBaseUrl: `${baseURL}/targets`,
        metadataDir: cachePath,
        targetDir: path_1.default.join(cachePath, 'targets'),
        config,
    });
}
