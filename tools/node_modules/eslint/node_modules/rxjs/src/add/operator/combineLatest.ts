
import { Observable } from '../../Observable';
import { combineLatest } from '../../operator/combineLatest';

Observable.prototype.combineLatest = combineLatest;

declare module '../../Observable' {
  interface Observable<T> {
    combineLatest: typeof combineLatest;
  }
}