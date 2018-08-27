import { Observable } from '../../Observable';
import { empty as staticEmpty } from '../../observable/empty';

Observable.empty = staticEmpty;

declare module '../../Observable' {
  namespace Observable {
    export let empty: typeof staticEmpty;
  }
}