import { Observable } from '../../Observable';
import { pairs as staticPairs } from '../../observable/pairs';

Observable.pairs = staticPairs;

declare module '../../Observable' {
  namespace Observable {
    export let pairs: typeof staticPairs;
  }
}