
import { Observable } from '../../Observable';
import { startWith } from '../../operator/startWith';

Observable.prototype.startWith = startWith;

declare module '../../Observable' {
  interface Observable<T> {
    startWith: typeof startWith;
  }
}