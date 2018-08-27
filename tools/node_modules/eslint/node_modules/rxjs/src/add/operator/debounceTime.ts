
import { Observable } from '../../Observable';
import { debounceTime } from '../../operator/debounceTime';

Observable.prototype.debounceTime = debounceTime;

declare module '../../Observable' {
  interface Observable<T> {
    debounceTime: typeof debounceTime;
  }
}