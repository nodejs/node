import { Observable } from '../Observable';

export function scalar<T>(value: T) {
  const result = new Observable<T>(subscriber => {
    subscriber.next(value);
    subscriber.complete();
  });
  result._isScalar = true;
  (result as any).value = value;
  return result;
}
