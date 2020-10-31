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

import * as m from 'mithril';

import {Actions, DeferredAction} from '../common/actions';

interface RouteMap {
  [route: string]: m.Component;
}

export const ROUTE_PREFIX = '#!';

export class Router {
  constructor(
      private defaultRoute: string, private routes: RouteMap,
      private dispatch: (a: DeferredAction) => void) {
    if (!(defaultRoute in routes)) {
      throw Error('routes must define a component for defaultRoute.');
    }

    window.onhashchange = () => this.navigateToCurrentHash();
  }

  /**
   * Parses and returns the current route string from |window.location.hash|.
   * May return routes that are not defined in |this.routes|.
   */
  getRouteFromHash(): string {
    const prefixLength = ROUTE_PREFIX.length;
    const hash = window.location.hash;

    // Do not try to parse route if prefix doesn't match.
    if (hash.substring(0, prefixLength) !== ROUTE_PREFIX) return '';

    return hash.split('?')[0].substring(prefixLength);
  }

  /**
   * Sets |route| on |window.location.hash|. If |route| if not defined in
   * |this.routes|, dispatches a navigation to |this.defaultRoute|.
   */
  setRouteOnHash(route: string) {
    history.pushState(undefined, "", ROUTE_PREFIX + route);

    if (!(route in this.routes)) {
      console.info(
          `Route ${route} not known redirecting to ${this.defaultRoute}.`);
      this.dispatch(Actions.navigate({route: this.defaultRoute}));
    }
  }

  /**
   * Dispatches navigation action to |this.getRouteFromHash()| if that is
   * defined in |this.routes|, otherwise to |this.defaultRoute|.
   */
  navigateToCurrentHash() {
    const hashRoute = this.getRouteFromHash();
    const newRoute = hashRoute in this.routes ? hashRoute : this.defaultRoute;
    this.dispatch(Actions.navigate({route: newRoute}));
    // TODO(dproy): Handle case when new route has a permalink.
  }

  /**
   * Returns the component for given |route|. If |route| is not defined, returns
   * component of |this.defaultRoute|.
   */
  resolve(route: string|null): m.Component {
    if (!route || !(route in this.routes)) {
      return this.routes[this.defaultRoute];
    }
    return this.routes[route];
  }

  static param(key: string) {
    const hash = window.location.hash;
    const paramStart = hash.indexOf('?');
    if (paramStart === -1) return undefined;
    return m.parseQueryString(hash.substring(paramStart))[key];
  }
}
