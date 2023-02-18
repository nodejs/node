"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.PolicyError = exports.InternalError = exports.ValidationError = exports.VerificationError = void 0;
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
/* eslint-disable @typescript-eslint/no-explicit-any */
class BaseError extends Error {
    constructor(message, cause) {
        super(message);
        this.name = this.constructor.name;
        this.cause = cause;
    }
}
class VerificationError extends BaseError {
}
exports.VerificationError = VerificationError;
class ValidationError extends BaseError {
}
exports.ValidationError = ValidationError;
class InternalError extends BaseError {
}
exports.InternalError = InternalError;
class PolicyError extends BaseError {
}
exports.PolicyError = PolicyError;
