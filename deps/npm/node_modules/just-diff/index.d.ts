// Definitions by: Cameron Hunter <https://github.com/cameronhunter> 
// Modified by: Angus Croll <https://github.com/angus-c>
type Operation = "add" | "replace" | "remove";

type JSONPatchPathConverter<OUTPUT> = (
  arrayPath: Array<string | number>
) => OUTPUT;

export function diff(
  a: object | Array<any>,
  b: object | Array<any>,
): Array<{ op: Operation; path: Array<string | number>; value: any }>;

export function diff<PATH>(
  a: object | Array<any>,
  b: object | Array<any>,
  jsonPatchPathConverter: JSONPatchPathConverter<PATH>
): Array<{ op: Operation; path: PATH; value: any }>;

export const jsonPatchPathConverter: JSONPatchPathConverter<string>;