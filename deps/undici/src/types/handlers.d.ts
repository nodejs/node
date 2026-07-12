import Dispatcher from './dispatcher'

export declare class RedirectHandler implements Dispatcher.DispatchHandler {
  static buildDispatch (dispatcher: Dispatcher, maxRedirections: number): Dispatcher.Dispatch

  constructor (
    dispatch: Dispatcher.Dispatch,
    maxRedirections: number,
    opts: Dispatcher.DispatchOptions,
    handler: Dispatcher.DispatchHandler
  )
}

export declare class DecoratorHandler implements Dispatcher.DispatchHandler {
  constructor (handler: Dispatcher.DispatchHandler)
}
