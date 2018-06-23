
import { Observable } from '../../Observable';
import { dematerialize } from '../../operator/dematerialize';

Observable.prototype.dematerialize = dematerialize;

declare module '../../Observable' {
  interface Observable<T> {
    dematerialize: typeof dematerialize;
  }
}