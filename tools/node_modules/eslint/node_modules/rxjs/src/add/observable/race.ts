import { Observable } from '../../Observable';
import { race as staticRace } from '../../observable/race';

Observable.race = staticRace;

declare module '../../Observable' {
  namespace Observable {
    export let race: typeof staticRace;
  }
}