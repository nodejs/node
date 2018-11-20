export function isPromise(value: any): value is PromiseLike<any> {
  return value && typeof (<any>value).subscribe !== 'function' && typeof (value as any).then === 'function';
}
