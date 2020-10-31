// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

export type ErrorHandler = (err: string) => void;

let errorHandler: ErrorHandler = (_: string) => {};

export function assertExists<A>(value: A | null | undefined): A {
  if (value === null || value === undefined) {
    throw new Error('Value doesn\'t exist');
  }
  return value;
}

export function assertTrue(value: boolean, optMsg?: string) {
  if (value !== true) {
    throw new Error(optMsg ? optMsg : 'Failed assertion');
  }
}

export function assertFalse(value: boolean, optMsg?: string) {
  assertTrue(!value, optMsg);
}

export function setErrorHandler(handler: ErrorHandler) {
  errorHandler = handler;
}

export function reportError(err: ErrorEvent|PromiseRejectionEvent|{}) {
  let errLog = '';
  let errorObj = undefined;

  if (err instanceof ErrorEvent) {
    errLog = err.message;
    errorObj = err.error;
  } else if (err instanceof PromiseRejectionEvent) {
    errLog = `${err.reason}`;
    errorObj = err.reason;
  } else {
    errLog = `${err}`;
  }
  if (errorObj !== undefined) {
    const errStack = (errorObj as {stack?: string}).stack;
    errLog += '\n';
    errLog += errStack !== undefined ? errStack : JSON.stringify(errorObj);
  }
  errLog += `\n\nUA: ${navigator.userAgent}\n`;

  console.error(errLog, err);
  errorHandler(errLog);
}