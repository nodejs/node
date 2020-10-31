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

import {
  Child,
  Controller,
} from './controller';

const _onCreate = jest.fn();
const _onDestroy = jest.fn();
const _run = jest.fn();

type MockStates = 'idle'|'state1'|'state2'|'state3';
class MockController extends Controller<MockStates> {
  constructor(public type: string) {
    super('idle');
    _onCreate(this.type);
  }

  run() {
    return _run(this.type);
  }

  onDestroy() {
    return _onDestroy(this.type);
  }
}

function runControllerTree(rootController: MockController): void {
  for (let runAgain = true, i = 0; runAgain; i++) {
    if (i >= 100) throw new Error('Controller livelock');
    runAgain = rootController.invoke();
  }
}

beforeEach(() => {
  _onCreate.mockClear();
  _onCreate.mockReset();
  _onDestroy.mockClear();
  _onDestroy.mockReset();
  _run.mockClear();
  _run.mockReset();
});

test('singleControllerNoTransition', () => {
  const rootCtl = new MockController('root');
  runControllerTree(rootCtl);
  expect(_run).toHaveBeenCalledTimes(1);
  expect(_run).toHaveBeenCalledWith('root');
});

test('singleControllerThreeTransitions', () => {
  const rootCtl = new MockController('root');
  _run.mockImplementation(() => {
    if (rootCtl.state === 'idle') {
      rootCtl.setState('state1');
    } else if (rootCtl.state === 'state1') {
      rootCtl.setState('state2');
    }
  });
  runControllerTree(rootCtl);
  expect(_run).toHaveBeenCalledTimes(3);
  expect(_run).toHaveBeenCalledWith('root');
});

test('nestedControllers', () => {
  const rootCtl = new MockController('root');
  let nextState: MockStates = 'idle';
  _run.mockImplementation((type: string) => {
    if (type !== 'root') return;
    rootCtl.setState(nextState);
    if (rootCtl.state === 'idle') return;

    if (rootCtl.state === 'state1') {
      return [
        Child('child1', MockController, 'child1'),
      ];
    }
    if (rootCtl.state === 'state2') {
      return [
        Child('child1', MockController, 'child1'),
        Child('child2', MockController, 'child2'),
      ];
    }
    if (rootCtl.state === 'state3') {
      return [
        Child('child1', MockController, 'child1'),
        Child('child3', MockController, 'child3'),
      ];
    }
    throw new Error('Not reached');
  });
  runControllerTree(rootCtl);
  expect(_run).toHaveBeenCalledWith('root');
  expect(_run).toHaveBeenCalledTimes(1);

  // Transition the root controller to state1. This will create the first child
  // and re-run both (because of the idle -> state1 transition).
  _run.mockClear();
  _onCreate.mockClear();
  nextState = 'state1';
  runControllerTree(rootCtl);
  expect(_onCreate).toHaveBeenCalledWith('child1');
  expect(_onCreate).toHaveBeenCalledTimes(1);
  expect(_run).toHaveBeenCalledWith('root');
  expect(_run).toHaveBeenCalledWith('child1');
  expect(_run).toHaveBeenCalledTimes(4);

  // Transition the root controller to state2. This will create the 2nd child
  // and run the three of them (root + 2 chilren) two times.
  _run.mockClear();
  _onCreate.mockClear();
  nextState = 'state2';
  runControllerTree(rootCtl);
  expect(_onCreate).toHaveBeenCalledWith('child2');
  expect(_onCreate).toHaveBeenCalledTimes(1);
  expect(_run).toHaveBeenCalledWith('root');
  expect(_run).toHaveBeenCalledWith('child1');
  expect(_run).toHaveBeenCalledWith('child2');
  expect(_run).toHaveBeenCalledTimes(6);

  // Transition the root controller to state3. This will create the 3rd child
  // and remove the 2nd one.
  _run.mockClear();
  _onCreate.mockClear();
  nextState = 'state3';
  runControllerTree(rootCtl);
  expect(_onCreate).toHaveBeenCalledWith('child3');
  expect(_onDestroy).toHaveBeenCalledWith('child2');
  expect(_onCreate).toHaveBeenCalledTimes(1);
  expect(_run).toHaveBeenCalledWith('root');
  expect(_run).toHaveBeenCalledWith('child1');
  expect(_run).toHaveBeenCalledWith('child3');
  expect(_run).toHaveBeenCalledTimes(6);

  // Finally transition back to the idle state. All children should be removed.
  _run.mockClear();
  _onCreate.mockClear();
  _onDestroy.mockClear();
  nextState = 'idle';
  runControllerTree(rootCtl);
  expect(_onDestroy).toHaveBeenCalledWith('child1');
  expect(_onDestroy).toHaveBeenCalledWith('child3');
  expect(_onCreate).toHaveBeenCalledTimes(0);
  expect(_onDestroy).toHaveBeenCalledTimes(2);
  expect(_run).toHaveBeenCalledWith('root');
  expect(_run).toHaveBeenCalledTimes(2);
});
