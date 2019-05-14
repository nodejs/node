import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Observable } from '../Observable';
import { OperatorFunction } from '../types';

export function isEmpty<T>(): OperatorFunction<T, boolean> {
  return (source: Observable<T>) => source.lift(new IsEmptyOperator());
}

class IsEmptyOperator implements Operator<any, boolean> {
  call (observer: Subscriber<boolean>, source: any): any {
    return source.subscribe(new IsEmptySubscriber(observer));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class IsEmptySubscriber extends Subscriber<any> {
  constructor(destination: Subscriber<boolean>) {
    super(destination);
  }

  private notifyComplete(isEmpty: boolean): void {
    const destination = this.destination;

    destination.next(isEmpty);
    destination.complete();
  }

  protected _next(value: boolean) {
    this.notifyComplete(false);
  }

  protected _complete() {
    this.notifyComplete(true);
  }
}
