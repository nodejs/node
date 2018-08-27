
import { Observable } from '../../Observable';
import { mergeScan } from '../../operator/mergeScan';

Observable.prototype.mergeScan = mergeScan;

declare module '../../Observable' {
  interface Observable<T> {
    mergeScan: typeof mergeScan;
  }
}