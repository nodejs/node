// @flow

import gensync from "gensync";

import loadConfig, { type InputOptions, type ResolvedConfig } from "./config";
import {
  run,
  type FileResult,
  type FileResultCallback,
} from "./transformation";
import * as fs from "./gensync-utils/fs";

import typeof * as transformFileBrowserType from "./transform-file-browser";
import typeof * as transformFileType from "./transform-file";

// Kind of gross, but essentially asserting that the exports of this module are the same as the
// exports of transform-file-browser, since this file may be replaced at bundle time with
// transform-file-browser.
((({}: any): $Exact<transformFileBrowserType>): $Exact<transformFileType>);

type TransformFile = {
  (filename: string, callback: FileResultCallback): void,
  (filename: string, opts: ?InputOptions, callback: FileResultCallback): void,
};

const transformFileRunner = gensync<[string, ?InputOptions], FileResult | null>(
  function* (filename, opts) {
    const options = { ...opts, filename };

    const config: ResolvedConfig | null = yield* loadConfig(options);
    if (config === null) return null;

    const code = yield* fs.readFile(filename, "utf8");
    return yield* run(config, code);
  },
);

export const transformFile: TransformFile = transformFileRunner.errback;
export const transformFileSync = transformFileRunner.sync;
export const transformFileAsync = transformFileRunner.async;
