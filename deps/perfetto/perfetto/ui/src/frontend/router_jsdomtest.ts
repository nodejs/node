// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {dingus} from 'dingusjs';

import {Actions, DeferredAction} from '../common/actions';

import {Router} from './router';

const mockComponent = {
  view() {}
};

const fakeDispatch = () => {};

beforeEach(() => {
  window.onhashchange = null;
  window.location.hash = '';
});

test('Default route must be defined', () => {
  expect(() => new Router('/a', {'/b': mockComponent}, fakeDispatch)).toThrow();
});

test('Resolves empty route to default component', () => {
  const router = new Router('/a', {'/a': mockComponent}, fakeDispatch);
  expect(router.resolve('')).toBe(mockComponent);
  expect(router.resolve(null)).toBe(mockComponent);
});

test('Parse route from hash', () => {
  const router = new Router('/', {'/': mockComponent}, fakeDispatch);
  window.location.hash = '#!/foobar?s=42';
  expect(router.getRouteFromHash()).toBe('/foobar');

  window.location.hash = '/foobar';  // Invalid prefix.
  expect(router.getRouteFromHash()).toBe('');
});

test('Set valid route on hash', () => {
  const dispatch = dingus<(a: DeferredAction) => void>();
  const router = new Router(
      '/',
      {
        '/': mockComponent,
        '/a': mockComponent,
      },
      dispatch);
  const prevHistoryLength = window.history.length;

  router.setRouteOnHash('/a');
  expect(window.location.hash).toBe('#!/a');
  expect(window.history.length).toBe(prevHistoryLength + 1);
  // No navigation action should be dispatched.
  expect(dispatch.calls.length).toBe(0);
});

test('Redirects to default for invalid route in setRouteOnHash ', () => {
  const dispatch = dingus<(a: DeferredAction) => void>();
  // const dispatch = () => {console.log("action received")};

  const router = new Router('/', {'/': mockComponent}, dispatch);
  router.setRouteOnHash('foo');
  expect(dispatch.calls.length).toBe(1);
  expect(dispatch.calls[0][1].length).toBeGreaterThanOrEqual(1);
  expect(dispatch.calls[0][1][0]).toEqual(Actions.navigate({route: '/'}));
});

test('Navigate on hash change', done => {
  const mockDispatch = (a: DeferredAction) => {
    expect(a).toEqual(Actions.navigate({route: '/viewer'}));
    done();
  };
  new Router(
      '/',
      {
        '/': mockComponent,
        '/viewer': mockComponent,
      },
      mockDispatch);
  window.location.hash = '#!/viewer';
});

test('Redirects to default when invalid route set in window location', done => {
  const mockDispatch = (a: DeferredAction) => {
    expect(a).toEqual(Actions.navigate({route: '/'}));
    done();
  };

  new Router(
      '/',
      {
        '/': mockComponent,
        '/viewer': mockComponent,
      },
      mockDispatch);

  window.location.hash = '#invalid';
});

test('navigateToCurrentHash with valid current route', () => {
  const dispatch = dingus<(a: DeferredAction) => void>();
  window.location.hash = '#!/b';
  const router =
      new Router('/', {'/': mockComponent, '/b': mockComponent}, dispatch);
  router.navigateToCurrentHash();
  expect(dispatch.calls[0][1][0]).toEqual(Actions.navigate({route: '/b'}));
});

test('navigateToCurrentHash with invalid current route', () => {
  const dispatch = dingus<(a: DeferredAction) => void>();
  window.location.hash = '#!/invalid';
  const router = new Router('/', {'/': mockComponent}, dispatch);
  router.navigateToCurrentHash();
  expect(dispatch.calls[0][1][0]).toEqual(Actions.navigate({route: '/'}));
});

test('Params parsing', () => {
  window.location.hash = '#!/foo?a=123&b=42&c=a?b?c';
  expect(Router.param('a')).toBe('123');
  expect(Router.param('b')).toBe('42');
  expect(Router.param('c')).toBe('a?b?c');
});
