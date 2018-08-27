export function isPromise<T>(value: any | Promise<T>): value is Promise<T> {
  return value && typeof (<any>value).subscribe !== 'function' && typeof (value as any).then === 'function';
}
