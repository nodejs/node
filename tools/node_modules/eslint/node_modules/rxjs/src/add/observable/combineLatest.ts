import { Observable } from '../../Observable';
import { combineLatest as combineLatestStatic } from '../../observable/combineLatest';

Observable.combineLatest = combineLatestStatic;

declare module '../../Observable' {
  namespace Observable {
    export let combineLatest: typeof combineLatestStatic;
  }
}