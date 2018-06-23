import { Observable } from '../../Observable';
import { of as staticOf } from '../../observable/of';

Observable.of = staticOf;

declare module '../../Observable' {
  namespace Observable {
    export let of: typeof staticOf; //formOf an iceberg!
  }
}