// @flow

import type { Handler } from "gensync";

import type {
  ConfigFile,
  IgnoreFile,
  RelativeConfig,
  FilePackageData,
} from "./types";

import type { CallerMetadata } from "../validation/options";

export type { ConfigFile, IgnoreFile, RelativeConfig, FilePackageData };

// eslint-disable-next-line require-yield
export function* findConfigUpwards(
  rootDir: string, // eslint-disable-line no-unused-vars
): Handler<string | null> {
  return null;
}

// eslint-disable-next-line require-yield
export function* findPackageData(filepath: string): Handler<FilePackageData> {
  return {
    filepath,
    directories: [],
    pkg: null,
    isPackage: false,
  };
}

// eslint-disable-next-line require-yield
export function* findRelativeConfig(
  pkgData: FilePackageData, // eslint-disable-line no-unused-vars
  envName: string, // eslint-disable-line no-unused-vars
  caller: CallerMetadata | void, // eslint-disable-line no-unused-vars
): Handler<RelativeConfig> {
  return { pkg: null, config: null, ignore: null };
}

// eslint-disable-next-line require-yield
export function* findRootConfig(
  dirname: string, // eslint-disable-line no-unused-vars
  envName: string, // eslint-disable-line no-unused-vars
  caller: CallerMetadata | void, // eslint-disable-line no-unused-vars
): Handler<ConfigFile | null> {
  return null;
}

// eslint-disable-next-line require-yield
export function* loadConfig(
  name: string,
  dirname: string,
  envName: string, // eslint-disable-line no-unused-vars
  caller: CallerMetadata | void, // eslint-disable-line no-unused-vars
): Handler<ConfigFile> {
  throw new Error(`Cannot load ${name} relative to ${dirname} in a browser`);
}

// eslint-disable-next-line require-yield
export function* resolveShowConfigPath(
  dirname: string, // eslint-disable-line no-unused-vars
): Handler<string | null> {
  return null;
}

export const ROOT_CONFIG_FILENAMES = [];

// eslint-disable-next-line no-unused-vars
export function resolvePlugin(name: string, dirname: string): string | null {
  return null;
}

// eslint-disable-next-line no-unused-vars
export function resolvePreset(name: string, dirname: string): string | null {
  return null;
}

export function loadPlugin(
  name: string,
  dirname: string,
): { filepath: string, value: mixed } {
  throw new Error(
    `Cannot load plugin ${name} relative to ${dirname} in a browser`,
  );
}

export function loadPreset(
  name: string,
  dirname: string,
): { filepath: string, value: mixed } {
  throw new Error(
    `Cannot load preset ${name} relative to ${dirname} in a browser`,
  );
}
