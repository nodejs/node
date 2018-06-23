import { Observable } from '../../Observable';
import { onErrorResumeNext as staticOnErrorResumeNext } from '../../observable/onErrorResumeNext';

Observable.onErrorResumeNext = staticOnErrorResumeNext;

declare module '../../Observable' {
  namespace Observable {
    export let onErrorResumeNext: typeof staticOnErrorResumeNext;
  }
}