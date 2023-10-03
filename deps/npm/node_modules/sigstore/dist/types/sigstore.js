"use strict";
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
Object.defineProperty(exports, "__esModule", { value: true });
exports.isCAVerificationOptions = exports.SubjectAlternativeNameType = void 0;
// Enums from protobuf-specs
var protobuf_specs_1 = require("@sigstore/protobuf-specs");
Object.defineProperty(exports, "SubjectAlternativeNameType", { enumerable: true, get: function () { return protobuf_specs_1.SubjectAlternativeNameType; } });
function isCAVerificationOptions(options) {
    return (options.ctlogOptions !== undefined &&
        (options.signers === undefined ||
            options.signers.$case === 'certificateIdentities'));
}
exports.isCAVerificationOptions = isCAVerificationOptions;
