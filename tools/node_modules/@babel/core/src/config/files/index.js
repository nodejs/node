// @flow

import typeof * as indexBrowserType from "./index-browser";
import typeof * as indexType from "./index";

// Kind of gross, but essentially asserting that the exports of this module are the same as the
// exports of index-browser, since this file may be replaced at bundle time with index-browser.
((({}: any): $Exact<indexBrowserType>): $Exact<indexType>);

export { findPackageData } from "./package";

export {
  findConfigUpwards,
  findRelativeConfig,
  findRootConfig,
  loadConfig,
  resolveShowConfigPath,
  ROOT_CONFIG_FILENAMES,
} from "./configuration";
export type {
  ConfigFile,
  IgnoreFile,
  RelativeConfig,
  FilePackageData,
} from "./types";
export {
  resolvePlugin,
  resolvePreset,
  loadPlugin,
  loadPreset,
} from "./plugins";
