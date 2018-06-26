
import { Observable } from '../../Observable';
import { zipAll } from '../../operator/zipAll';

Observable.prototype.zipAll = zipAll;

declare module '../../Observable' {
  interface Observable<T> {
    zipAll: typeof zipAll;
  }
}