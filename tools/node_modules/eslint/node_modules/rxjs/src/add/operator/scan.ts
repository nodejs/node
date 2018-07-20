
import { Observable } from '../../Observable';

import { scan } from '../../operator/scan';

Observable.prototype.scan = scan;

declare module '../../Observable' {
  interface Observable<T> {
    scan: typeof scan;
  }
}