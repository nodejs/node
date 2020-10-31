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

// tslint:disable-next-line no-any
export type ControllerAny = Controller</*StateType=*/any>;

export interface ControllerFactory<ConstructorArgs> {
  new(args: ConstructorArgs): ControllerAny;
}

interface ControllerInitializer<ConstructorArgs> {
  id: string;
  factory: ControllerFactory<ConstructorArgs>;
  args: ConstructorArgs;
}

// tslint:disable-next-line no-any
export type ControllerInitializerAny = ControllerInitializer<any>;

export function Child<ConstructorArgs>(
    id: string,
    factory: ControllerFactory<ConstructorArgs>,
    args: ConstructorArgs): ControllerInitializer<ConstructorArgs> {
  return {id, factory, args};
}

export type Children = ControllerInitializerAny[];

export abstract class Controller<StateType> {
  // This is about the local FSM state, has nothing to do with the global
  // app state.
  private _stateChanged = false;
  private _inRunner = false;
  private _state: StateType;
  private _children = new Map<string, ControllerAny>();

  constructor(initialState: StateType) {
    this._state = initialState;
  }

  abstract run(): Children|void;
  onDestroy(): void {}

  // Invokes the current controller subtree, recursing into children.
  // While doing so handles lifecycle of child controllers.
  // This method should be called only by the runControllers() method in
  // globals.ts. Exposed publicly for testing.
  invoke(): boolean {
    if (this._inRunner) throw new Error('Reentrancy in Controller');
    this._stateChanged = false;
    this._inRunner = true;
    const resArray = this.run();
    let triggerAnotherRun = this._stateChanged;
    this._stateChanged = false;

    const nextChildren = new Map<string, ControllerInitializerAny>();
    if (resArray !== undefined) {
      for (const childConfig of resArray) {
        if (nextChildren.has(childConfig.id)) {
          throw new Error(`Duplicate children controller ${childConfig.id}`);
        }
        nextChildren.set(childConfig.id, childConfig);
      }
    }
    const dtors = new Array<(() => void)>();
    const runners = new Array<(() => boolean)>();
    for (const key of this._children.keys()) {
      if (nextChildren.has(key)) continue;
      const instance = this._children.get(key)!;
      this._children.delete(key);
      dtors.push(() => instance.onDestroy());
    }
    for (const nextChild of nextChildren.values()) {
      if (!this._children.has(nextChild.id)) {
        const instance = new nextChild.factory(nextChild.args);
        this._children.set(nextChild.id, instance);
      }
      const instance = this._children.get(nextChild.id)!;
      runners.push(() => instance.invoke());
    }

    for (const dtor of dtors) dtor();  // Invoke all onDestroy()s.

    // Invoke all runner()s.
    for (const runner of runners) {
      const recursiveRes = runner();
      triggerAnotherRun = triggerAnotherRun || recursiveRes;
    }

    this._inRunner = false;
    return triggerAnotherRun;
  }

  setState(state: StateType) {
    if (!this._inRunner) {
      throw new Error('Cannot setState() outside of the run() method');
    }
    this._stateChanged = state !== this._state;
    this._state = state;
  }

  get state(): StateType {
    return this._state;
  }
}
