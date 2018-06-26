
import { Observable } from '../../Observable';
import { _catch } from '../../operator/catch';

Observable.prototype.catch = _catch;
Observable.prototype._catch = _catch;

declare module '../../Observable' {
  interface Observable<T> {
    catch: typeof _catch;
    _catch: typeof _catch;
  }
}