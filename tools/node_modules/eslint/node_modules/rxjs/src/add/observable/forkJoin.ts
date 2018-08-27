import { Observable } from '../../Observable';
import { forkJoin as staticForkJoin } from '../../observable/forkJoin';

Observable.forkJoin = staticForkJoin;

declare module '../../Observable' {
  namespace Observable {
    export let forkJoin: typeof staticForkJoin;
  }
}