import Dispatcher from'./types/dispatcher'
import { setGlobalDispatcher, getGlobalDispatcher } from './types/global-dispatcher'
import { setGlobalOrigin, getGlobalOrigin } from './types/global-origin'
import Pool from'./types/pool'
import { RedirectHandler, DecoratorHandler } from './types/handlers'

import BalancedPool from './types/balanced-pool'
import Client from'./types/client'
import buildConnector from'./types/connector'
import errors from'./types/errors'
import Agent from'./types/agent'
import MockClient from'./types/mock-client'
import MockPool from'./types/mock-pool'
import MockAgent from'./types/mock-agent'
import mockErrors from'./types/mock-errors'
import ProxyAgent from'./types/proxy-agent'
import { request, pipeline, stream, connect, upgrade } from './types/api'

export * from './types/cookies'
export * from './types/fetch'
export * from './types/file'
export * from './types/filereader'
export * from './types/formdata'
export * from './types/diagnostics-channel'
export * from './types/websocket'
export * from './types/content-type'
export * from './types/cache'
export { Interceptable } from './types/mock-interceptor'

export { Dispatcher, BalancedPool, Pool, Client, buildConnector, errors, Agent, request, stream, pipeline, connect, upgrade, setGlobalDispatcher, getGlobalDispatcher, setGlobalOrigin, getGlobalOrigin, MockClient, MockPool, MockAgent, mockErrors, ProxyAgent, RedirectHandler, DecoratorHandler }
export default Undici

declare namespace Undici {
  var Dispatcher: typeof import('./types/dispatcher').default
  var Pool: typeof import('./types/pool').default;
  var RedirectHandler: typeof import ('./types/handlers').RedirectHandler
  var DecoratorHandler: typeof import ('./types/handlers').DecoratorHandler
  var createRedirectInterceptor: typeof import ('./types/interceptors').createRedirectInterceptor
  var BalancedPool: typeof import('./types/balanced-pool').default;
  var Client: typeof import('./types/client').default;
  var buildConnector: typeof import('./types/connector').default;
  var errors: typeof import('./types/errors').default;
  var Agent: typeof import('./types/agent').default;
  var setGlobalDispatcher: typeof import('./types/global-dispatcher').setGlobalDispatcher;
  var getGlobalDispatcher: typeof import('./types/global-dispatcher').getGlobalDispatcher;
  var request: typeof import('./types/api').request;
  var stream: typeof import('./types/api').stream;
  var pipeline: typeof import('./types/api').pipeline;
  var connect: typeof import('./types/api').connect;
  var upgrade: typeof import('./types/api').upgrade;
  var MockClient: typeof import('./types/mock-client').default;
  var MockPool: typeof import('./types/mock-pool').default;
  var MockAgent: typeof import('./types/mock-agent').default;
  var mockErrors: typeof import('./types/mock-errors').default;
  var fetch: typeof import('./types/fetch').fetch;
  var caches: typeof import('./types/cache').caches;
}
