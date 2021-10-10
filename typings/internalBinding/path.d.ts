declare function InternalBinding(binding: 'path'): {
  basename(path: string, ext?: string): string;
  dirname(path: string): string;
  extname(path: string): string;
  format(pathObject: {dir?: string, root?: string, base?: string, name?: string, ext?: string}): string;
  isAbsolute(path: string): boolean;
  join(...paths: string[]): string;
  normalize(path: string): string;
  parse(path: string): {dir: string, root: string, base: string, name: string, ext: string};
  relative(from: string, to: string): string;
  resolve(...paths: string[]): string;
  toNamespacedPath(path: string): string;
};
