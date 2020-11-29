// @flow

// duplicated from transform-file so we do not have to import anything here
type TransformFile = {
  (filename: string, callback: Function): void,
  (filename: string, opts: ?Object, callback: Function): void,
};

export const transformFile: TransformFile = (function transformFile(
  filename,
  opts,
  callback,
) {
  if (typeof opts === "function") {
    callback = opts;
  }

  callback(new Error("Transforming files is not supported in browsers"), null);
}: Function);

export function transformFileSync() {
  throw new Error("Transforming files is not supported in browsers");
}

export function transformFileAsync() {
  return Promise.reject(
    new Error("Transforming files is not supported in browsers"),
  );
}
