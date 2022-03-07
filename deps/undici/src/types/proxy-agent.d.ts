import Agent = require('./agent')
import Dispatcher = require('./dispatcher')

export = ProxyAgent

declare class ProxyAgent extends Dispatcher {
  constructor(options: ProxyAgent.Options | string)

  dispatch(options: Agent.DispatchOptions, handler: Dispatcher.DispatchHandlers): boolean;
  close(): Promise<void>;
}

declare namespace ProxyAgent {
  export interface Options extends Agent.Options {
    uri: string;
  }
}
