
import { Observable } from '../../Observable';
import { publish } from '../../operator/publish';

Observable.prototype.publish = <any>publish;

declare module '../../Observable' {
  interface Observable<T> {
    publish: typeof publish;
  }
}