
import { Observable } from '../../Observable';
import { isEmpty } from '../../operator/isEmpty';

Observable.prototype.isEmpty = isEmpty;

declare module '../../Observable' {
  interface Observable<T> {
    isEmpty: typeof isEmpty;
  }
}