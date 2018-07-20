import { Observable } from '../../Observable';
import { timer as staticTimer } from '../../observable/timer';

Observable.timer = staticTimer;

declare module '../../Observable' {
  namespace Observable {
    export let timer: typeof staticTimer;
  }
}