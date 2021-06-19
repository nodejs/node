import type { ValidatedOptions } from "./validation/options";
import getTargets from "@babel/helper-compilation-targets";

import type { Targets } from "@babel/helper-compilation-targets";

export function resolveBrowserslistConfigFile(
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  browserslistConfigFile: string,
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  configFilePath: string,
): string | void {
  return undefined;
}

export function resolveTargets(
  options: ValidatedOptions,
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  root: string,
): Targets {
  // todo(flow->ts) remove any and refactor to not assign different types into same variable
  let targets: any = options.targets;
  if (typeof targets === "string" || Array.isArray(targets)) {
    targets = { browsers: targets };
  }
  if (targets && targets.esmodules) {
    targets = { ...targets, esmodules: "intersect" };
  }

  return getTargets(targets, {
    ignoreBrowserslistConfig: true,
    browserslistEnv: options.browserslistEnv,
  });
}
