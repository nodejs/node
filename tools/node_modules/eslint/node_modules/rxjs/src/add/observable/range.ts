import { Observable } from '../../Observable';
import { range as staticRange } from '../../observable/range';

Observable.range = staticRange;

declare module '../../Observable' {
  namespace Observable {
    export let range: typeof staticRange;
  }
}