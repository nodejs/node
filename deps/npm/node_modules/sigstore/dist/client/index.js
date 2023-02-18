"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Rekor = exports.Fulcio = void 0;
/*
Copyright 2022 The Sigstore Authors.

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
var fulcio_1 = require("./fulcio");
Object.defineProperty(exports, "Fulcio", { enumerable: true, get: function () { return fulcio_1.Fulcio; } });
var rekor_1 = require("./rekor");
Object.defineProperty(exports, "Rekor", { enumerable: true, get: function () { return rekor_1.Rekor; } });
