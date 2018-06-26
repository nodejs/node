
import { Observable } from '../../Observable';
import { shareReplay } from '../../operator/shareReplay';

Observable.prototype.shareReplay = shareReplay;

declare module '../../Observable' {
  interface Observable<T> {
    shareReplay: typeof shareReplay;
  }
}