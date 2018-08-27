import { AjaxCreationMethod } from '../../../observable/dom/AjaxObservable';
declare module '../../../Observable' {
    namespace Observable {
        let ajax: AjaxCreationMethod;
    }
}
