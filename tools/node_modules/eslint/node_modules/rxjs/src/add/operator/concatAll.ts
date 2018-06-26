
import { Observable } from '../../Observable';
import { concatAll } from '../../operator/concatAll';

Observable.prototype.concatAll = concatAll;

declare module '../../Observable' {
  interface Observable<T> {
    concatAll: typeof concatAll;
  }
}