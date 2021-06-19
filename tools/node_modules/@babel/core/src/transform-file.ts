import gensync from "gensync";

import loadConfig from "./config";
import type { InputOptions, ResolvedConfig } from "./config";
import { run } from "./transformation";
import type { FileResult, FileResultCallback } from "./transformation";
import * as fs from "./gensync-utils/fs";

type transformFileBrowserType = typeof import("./transform-file-browser");
type transformFileType = typeof import("./transform-file");

// Kind of gross, but essentially asserting that the exports of this module are the same as the
// exports of transform-file-browser, since this file may be replaced at bundle time with
// transform-file-browser.
({} as any as transformFileBrowserType as transformFileType);

type TransformFile = {
  (filename: string, callback: FileResultCallback): void;
  (
    filename: string,
    opts: InputOptions | undefined | null,
    callback: FileResultCallback,
  ): void;
};

const transformFileRunner = gensync<
  (filename: string, opts?: InputOptions) => FileResult | null
>(function* (filename, opts: InputOptions) {
  const options = { ...opts, filename };

  const config: ResolvedConfig | null = yield* loadConfig(options);
  if (config === null) return null;

  const code = yield* fs.readFile(filename, "utf8");
  return yield* run(config, code);
});

export const transformFile = transformFileRunner.errback as TransformFile;
export const transformFileSync = transformFileRunner.sync;
export const transformFileAsync = transformFileRunner.async;
