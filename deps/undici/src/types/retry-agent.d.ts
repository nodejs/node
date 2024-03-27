import Agent from './agent'
import buildConnector from './connector';
import Dispatcher from './dispatcher'
import { IncomingHttpHeaders } from './header'
import RetryHandler from './retry-handler'

export default RetryAgent

declare class RetryAgent extends Dispatcher {
  constructor(dispatcher: Dispatcher, options?: RetryHandler.RetryOptions)
}
