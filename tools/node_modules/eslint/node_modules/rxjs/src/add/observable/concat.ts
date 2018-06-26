import { Observable } from '../../Observable';
import { concat as concatStatic } from '../../observable/concat';

Observable.concat = concatStatic;

declare module '../../Observable' {
  namespace Observable {
    export let concat: typeof concatStatic;
  }
}