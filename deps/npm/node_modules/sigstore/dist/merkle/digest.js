"use strict";
/*
Copyright 2022 GitHub, Inc

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
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Hasher = void 0;
const crypto_1 = __importDefault(require("crypto"));
const RFC6962LeafHashPrefix = Buffer.from([0x00]);
const RFC6962NodeHashPrefix = Buffer.from([0x01]);
// Implements Merkle Tree Hash logic according to RFC6962.
// https://datatracker.ietf.org/doc/html/rfc6962#section-2
class Hasher {
    constructor(algorithm = 'sha256') {
        this.algorithm = algorithm;
    }
    size() {
        return crypto_1.default.createHash(this.algorithm).digest().length;
    }
    hashLeaf(leaf) {
        const hasher = crypto_1.default.createHash(this.algorithm);
        hasher.update(RFC6962LeafHashPrefix);
        hasher.update(leaf);
        return hasher.digest();
    }
    hashChildren(l, r) {
        const hasher = crypto_1.default.createHash(this.algorithm);
        hasher.update(RFC6962NodeHashPrefix);
        hasher.update(l);
        hasher.update(r);
        return hasher.digest();
    }
}
exports.Hasher = Hasher;
