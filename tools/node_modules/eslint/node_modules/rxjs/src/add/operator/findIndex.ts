
import { Observable } from '../../Observable';
import { findIndex } from '../../operator/findIndex';

Observable.prototype.findIndex = findIndex;

declare module '../../Observable' {
  interface Observable<T> {
    findIndex: typeof findIndex;
  }
}