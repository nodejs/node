
import { Observable } from '../../Observable';
import { partition } from '../../operator/partition';

Observable.prototype.partition = partition;

declare module '../../Observable' {
  interface Observable<T> {
    partition: typeof partition;
  }
}