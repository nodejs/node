// Definitions by: Eddie Atkinson <https://github.com/eddie-atkinson>

type Operation = "add" | "replace" | "remove" | "move";

type DiffOps = Array<{
  op: Operation;
  path: Array<string | number>;
  value?: any;
}>;
type PathConverter = (path: string) => string[];

export function diffApply<T extends object>(
  obj: T,
  diff: DiffOps,
  pathConverter?: PathConverter
): T;
export const jsonPatchPathConverter: PathConverter;
