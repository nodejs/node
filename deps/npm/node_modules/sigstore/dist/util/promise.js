"use strict";
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
Object.defineProperty(exports, "__esModule", { value: true });
exports.promiseAny = void 0;
// Implementation of Promise.any (not available until Node v15).
// We're basically inverting the logic of Promise.all and taking advantage
// of the fact that Promise.all will return early on the first rejection.
// By reversing the resolve/reject logic we can use this to return early
// on the first resolved promise.
const promiseAny = async (values) => {
    return Promise.all([...values].map((promise) => new Promise((resolve, reject) => promise.then(reject, resolve)))).then((errors) => Promise.reject(errors), (value) => Promise.resolve(value));
};
exports.promiseAny = promiseAny;
