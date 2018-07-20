
import { Observable } from '../../Observable';
import { window } from '../../operator/window';

Observable.prototype.window = window;

declare module '../../Observable' {
  interface Observable<T> {
    window: typeof window;
  }
}