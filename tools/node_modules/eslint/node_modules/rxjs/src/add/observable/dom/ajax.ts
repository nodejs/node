import { Observable } from '../../../Observable';
import { ajax as staticAjax } from '../../../observable/dom/ajax';
import { AjaxCreationMethod } from '../../../observable/dom/AjaxObservable';

Observable.ajax = staticAjax;

declare module '../../../Observable' {
  namespace Observable {
    export let ajax: AjaxCreationMethod;
  }
}