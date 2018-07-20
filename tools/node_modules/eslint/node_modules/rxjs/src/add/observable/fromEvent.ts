import { Observable } from '../../Observable';
import { fromEvent as staticFromEvent } from '../../observable/fromEvent';

Observable.fromEvent = staticFromEvent;

declare module '../../Observable' {
  namespace Observable {
    export let fromEvent: typeof staticFromEvent;
  }
}