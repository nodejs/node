"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.toDER = void 0;
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
const PEM_HEADER = /-----BEGIN (.*)-----/;
const PEM_FOOTER = /-----END (.*)-----/;
function toDER(certificate) {
    const lines = certificate
        .split('\n')
        .map((line) => line.match(PEM_HEADER) || line.match(PEM_FOOTER) ? '' : line);
    return Buffer.from(lines.join(''), 'base64');
}
exports.toDER = toDER;
