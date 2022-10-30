import Dispatcher = require('./types/dispatcher')
import { setGlobalDispatcher, getGlobalDispatcher } from './types/global-dispatcher'
import { setGlobalOrigin, getGlobalOrigin } from './types/global-origin'
import Pool = require('./types/pool')
import { RedirectHandler, DecoratorHandler } from './types/handlers'

import BalancedPool = require('./types/balanced-pool')
import Client = require('./types/client')
import buildConnector = require('./types/connector')
import errors = require('./types/errors')
import Agent = require('./types/agent')
import MockClient = require('./types/mock-client')
import MockPool = require('./types/mock-pool')
import MockAgent = require('./types/mock-agent')
import mockErrors = require('./types/mock-errors')
import ProxyAgent = require('./types/proxy-agent')
import { request, pipeline, stream, connect, upgrade } from './types/api'

export * from './types/fetch'
export * from './types/file'
export * from './types/filereader'
export * from './types/formdata'
export * from './types/diagnostics-channel'
export { Interceptable } from './types/mock-interceptor'

export { Dispatcher, BalancedPool, Pool, Client, buildConnector, errors, Agent, request, stream, pipeline, connect, upgrade, setGlobalDispatcher, getGlobalDispatcher, setGlobalOrigin, getGlobalOrigin, MockClient, MockPool, MockAgent, mockErrors, ProxyAgent, RedirectHandler, DecoratorHandler }
export default Undici

declare namespace Undici {
  var Dispatcher: typeof import('./types/dispatcher')
  var Pool: typeof import('./types/pool');
  var RedirectHandler: typeof import ('./types/handlers').RedirectHandler
  var DecoratorHandler: typeof import ('./types/handlers').DecoratorHandler
  var createRedirectInterceptor: typeof import ('./types/interceptors').createRedirectInterceptor
  var BalancedPool: typeof import('./types/balanced-pool');
  var Client: typeof import('./types/client');
  var buildConnector: typeof import('./types/connector');
  var errors: typeof import('./types/errors');
  var Agent: typeof import('./types/agent');
  var setGlobalDispatcher: typeof import('./types/global-dispatcher').setGlobalDispatcher;
  var getGlobalDispatcher: typeof import('./types/global-dispatcher').getGlobalDispatcher;
  var request: typeof import('./types/api').request;
  var stream: typeof import('./types/api').stream;
  var pipeline: typeof import('./types/api').pipeline;
  var connect: typeof import('./types/api').connect;
  var upgrade: typeof import('./types/api').upgrade;
  var MockClient: typeof import('./types/mock-client');
  var MockPool: typeof import('./types/mock-pool');
  var MockAgent: typeof import('./types/mock-agent');
  var mockErrors: typeof import('./types/mock-errors');
  var fetch: typeof import('./types/fetch').fetch;
}
