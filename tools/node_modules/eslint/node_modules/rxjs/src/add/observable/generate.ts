import { Observable } from '../../Observable';
import { generate as staticGenerate } from '../../observable/generate';

Observable.generate = staticGenerate;

declare module '../../Observable' {
  namespace Observable {
    export let generate: typeof staticGenerate;
  }
}