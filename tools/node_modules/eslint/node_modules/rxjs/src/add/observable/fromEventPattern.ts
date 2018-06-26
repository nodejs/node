import { Observable } from '../../Observable';
import { fromEventPattern as staticFromEventPattern } from '../../observable/fromEventPattern';

Observable.fromEventPattern = staticFromEventPattern;

declare module '../../Observable' {
  namespace Observable {
    export let fromEventPattern: typeof staticFromEventPattern;
  }
}