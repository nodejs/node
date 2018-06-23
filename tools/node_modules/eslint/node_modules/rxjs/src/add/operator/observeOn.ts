
import { Observable } from '../../Observable';
import { observeOn } from '../../operator/observeOn';

Observable.prototype.observeOn = observeOn;

declare module '../../Observable' {
  interface Observable<T> {
    observeOn: typeof observeOn;
  }
}