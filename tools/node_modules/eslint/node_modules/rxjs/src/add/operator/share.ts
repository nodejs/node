
import { Observable } from '../../Observable';
import { share } from '../../operator/share';

Observable.prototype.share = share;

declare module '../../Observable' {
  interface Observable<T> {
    share: typeof share;
  }
}