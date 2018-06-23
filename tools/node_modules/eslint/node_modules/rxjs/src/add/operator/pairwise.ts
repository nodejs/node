
import { Observable } from '../../Observable';
import { pairwise } from '../../operator/pairwise';

Observable.prototype.pairwise = pairwise;

declare module '../../Observable' {
  interface Observable<T> {
    pairwise: typeof pairwise;
  }
}