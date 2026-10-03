import contextlib
import copy
import io
import os
import socket
import sys
import threading
import unittest
from types import SimpleNamespace
from unittest import mock

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import test as runner


WAIT_TIMEOUT = 10


class SchedulerCase(runner.TestCase):
  def __init__(self, name, action=None, suite='sequential', parallel=False,
               arch='none', mode='release'):
    super().__init__(None, [suite, name], arch, mode)
    self.parallel = parallel
    self.action = action
    self.outcomes = {runner.PASS}
    self.calls = 0

  def IsNegative(self):
    return False

  def Run(self):
    self.calls += 1
    output = self.action(self) if self.action else None
    if output is None:
      output = runner.CommandOutput(0, False, '', '')
    return runner.TestOutput(self, ['node', '/'.join(self.path)], output, False)


class SchedulerProgress(runner.ProgressIndicator):
  def __init__(self, cases, flaky_tests_mode=runner.RUN, measure_flakiness=0,
               reported=None):
    super().__init__(cases, flaky_tests_mode, measure_flakiness)
    self.started = []
    self.completed = []
    self.reported = reported or {}

  def Starting(self):
    pass

  def Done(self):
    pass

  def AboutToRun(self, case):
    self.started.append(case)

  def HasRun(self, output):
    self.completed.append(output.test)
    event = self.reported.get(output.test)
    if event:
      event.set()


class EnvironmentCase(SchedulerCase):
  def __init__(self, name, action=None, suite='sequential', parallel=False):
    super().__init__(name, action, suite, parallel)
    self.environments = []

  def Run(self):
    self.calls += 1
    return runner.TestCase.Run(self)

  def GetRunConfiguration(self):
    return {'command': ['node', '/'.join(self.path)], 'envs': {'EXAMPLE': 'kept'}}

  def GetLabel(self):
    return '/'.join(self.path)

  def RunCommand(self, command, env):
    self.environments.append(env.copy())
    output = self.action(self, env) if self.action else None
    return runner.TestOutput(self, command, output or runner.CommandOutput(0, False, '', ''), False)


class ExecuteTest(unittest.TestCase):
  def test_invalid_utf8_output_preserves_test_result(self):
    context = SimpleNamespace(verbose=False, suppress_dialogs=False,
                              abort_on_timeout=False)
    code = '''import sys
sys.stdout.buffer.write(b'valid: \\xc3\\xa9\\ninvalid: \\xe8!\\n')
sys.stderr.buffer.write(b'valid: \\xe2\\x82\\xac\\ninvalid: \\xff!\\n')
sys.exit(7)
'''
    output = runner.Execute([sys.executable, '-c', code], context, timeout=5)
    self.assertEqual(output.stdout, 'valid: é\ninvalid: \ufffd!\n')
    self.assertEqual(output.stderr, 'valid: €\ninvalid: \ufffd!\n')
    self.assertEqual(output.exit_code, 7)
    self.assertFalse(output.timed_out)

    case = SchedulerCase('test-net-invalid-utf8')
    failure = runner.TestOutput(case, ['node', 'test-net-invalid-utf8'], output, False)
    self.assertTrue(failure.UnexpectedOutput())
    report = SchedulerProgress([case]).GetFailureOutput(failure)
    self.assertIn(output.stdout.strip(), report)
    self.assertIn(output.stderr.strip(), report)
    report.encode('utf8')


class SchedulerTest(unittest.TestCase):
  def wait_for(self, event):
    self.assertTrue(event.wait(WAIT_TIMEOUT), 'test runner did not make progress')

  @contextlib.contextmanager
  def running(self, progress, tasks=2, release=()):
    finished = threading.Event()
    result = {}
    errors = []

    def run():
      try:
        result.update(progress.Run(tasks))
      except BaseException as error:
        errors.append(error)
      finally:
        finished.set()

    thread = threading.Thread(target=run, daemon=True)
    thread.start()

    def complete():
      self.wait_for(finished)
      if errors:
        raise errors[0]
      return result

    try:
      yield complete
    finally:
      progress.Shutdown()
      for event in release:
        event.set()
      thread.join(WAIT_TIMEOUT)
      self.assertFalse(thread.is_alive(), 'test runner did not shut down')

  def test_distinct_subsystems_overlap_and_bypass_blocked_head(self):
    first_started = threading.Event()
    other_started = threading.Event()
    release = threading.Event()
    first_finished = threading.Event()

    def first(case):
      first_started.set()
      self.wait_for(other_started)
      self.wait_for(release)
      first_finished.set()

    def second(case):
      self.assertTrue(first_finished.is_set())

    def other(case):
      self.wait_for(first_started)
      other_started.set()
      self.wait_for(release)

    cases = [SchedulerCase('test-net-first', first),
             SchedulerCase('test-net-second', second),
             SchedulerCase('test-http-first', other)]
    progress = SchedulerProgress(cases)
    with self.running(progress, release=[release]) as complete:
      self.wait_for(other_started)
      self.assertNotIn(cases[1], progress.started)
      release.set()
      self.assertTrue(complete()['allPassed'])
    self.assertEqual(progress.remaining, 0)
    self.assertEqual([case.calls for case in cases], [1, 1, 1])

  def test_repeats_and_build_variants_preserve_subsystem_order(self):
    entered = []
    active = set()
    lock = threading.Lock()
    first_started = threading.Event()
    other_started = threading.Event()
    release = threading.Event()

    def action(case):
      with lock:
        self.assertNotIn('net', active)
        active.add('net')
        entered.append(case)
      if case is cases[0]:
        first_started.set()
        self.wait_for(other_started)
        self.wait_for(release)
      with lock:
        active.remove('net')

    def other(case):
      self.wait_for(first_started)
      other_started.set()

    cases = [SchedulerCase('test-net-repeat', action),
             SchedulerCase('test-net-repeat', action),
             SchedulerCase('test-net-repeat', action, mode='debug'),
             SchedulerCase('test-net-repeat', action, arch='arm64')]
    progress = SchedulerProgress(cases + [SchedulerCase('test-http-other', other)])
    with self.running(progress, release=[release]) as complete:
      self.wait_for(other_started)
      self.assertEqual(entered, cases[:1])
      release.set()
      self.assertTrue(complete()['allPassed'])
    self.assertEqual(entered, cases)

  def test_single_job_uses_calling_thread_and_preserves_order(self):
    calling_thread = threading.current_thread()
    calls = []

    def action(case):
      self.assertIs(threading.current_thread(), calling_thread)
      calls.append(case)

    cases = [SchedulerCase('test-net-first', action),
             SchedulerCase('test-http-first', action),
             SchedulerCase('test-net-second', action)]
    progress = SchedulerProgress(cases)
    with mock.patch.object(runner.threading, 'Thread') as worker:
      self.assertTrue(progress.Run(1)['allPassed'])
      worker.assert_not_called()
    self.assertEqual(calls, cases)
    self.assertEqual([case.thread_id for case in cases], [0, 0, 0])

  def test_parallel_then_legacy_then_grouped_sequential_phases(self):
    parallel_started = [threading.Event(), threading.Event()]
    parallel_finished = [threading.Event(), threading.Event()]
    release_parallel = threading.Event()
    legacy_started = threading.Event()
    legacy_finished = threading.Event()
    release_legacy = threading.Event()
    legacy_calls = []

    def parallel(index):
      def action(case):
        parallel_started[index].set()
        self.wait_for(release_parallel)
        parallel_finished[index].set()
      return action

    def legacy(case):
      self.assertTrue(all(event.is_set() for event in parallel_finished))
      self.assertEqual(case.thread_id, 0)
      legacy_calls.append(case)
      if len(legacy_calls) == 1:
        legacy_started.set()
        self.wait_for(release_legacy)
      else:
        legacy_finished.set()

    def sequential(case):
      self.assertTrue(legacy_finished.is_set())

    legacy_cases = [SchedulerCase('test-addon-first', legacy, suite='addons'),
                    SchedulerCase('test-addon-second', legacy, suite='node-api')]
    cases = [SchedulerCase('test-net-first', sequential), legacy_cases[0],
             SchedulerCase('test-parallel-first', parallel(0), suite='parallel', parallel=True),
             legacy_cases[1],
             SchedulerCase('test-parallel-second', parallel(1), suite='parallel', parallel=True),
             SchedulerCase('test-http-first', sequential)]
    progress = SchedulerProgress(cases)
    with self.running(progress, release=[release_parallel, release_legacy]) as complete:
      for event in parallel_started:
        self.wait_for(event)
      release_parallel.set()
      self.wait_for(legacy_started)
      release_legacy.set()
      self.assertTrue(complete()['allPassed'])
    self.assertEqual(legacy_calls, legacy_cases)

  def test_assigns_unique_serial_ids_and_bounded_worker_ids(self):
    cases = [SchedulerCase('test-net-%d' % index) for index in range(4)]
    cases += [SchedulerCase('test-http-%d' % index) for index in range(4)]
    cases += [SchedulerCase('test-parallel', suite='parallel', parallel=True),
              SchedulerCase('test-legacy', suite='pummel')]
    progress = SchedulerProgress(cases)
    self.assertTrue(progress.Run(3)['allPassed'])
    self.assertEqual(sorted(case.serial_id for case in cases), list(range(len(cases))))
    self.assertTrue(all(0 <= case.thread_id < 3 for case in cases))
    self.assertEqual(len(progress.completed), len(cases))
    self.assertTrue(all(case.duration is not None for case in cases))

  def test_worker_port_ranges_allow_distinct_subsystem_listeners_to_overlap(self):
    started = [threading.Event(), threading.Event()]
    release = threading.Event()
    # Select a base and check that its adjacent worker range is available.
    for _ in range(20):
      with socket.socket() as first, socket.socket() as second:
        first.bind(('127.0.0.1', 0))
        base = first.getsockname()[1]
        if base + 2 * runner.SEQUENTIAL_PORT_RANGE - 1 > 65535:
          continue
        try:
          second.bind(('127.0.0.1', base + runner.SEQUENTIAL_PORT_RANGE))
        except OSError:
          continue
        break
    else:
      self.fail('could not find two available worker port ranges')

    def listener(index):
      def action(case, env):
        with socket.socket() as server:
          port = int(env.get('NODE_COMMON_PORT', os.environ['NODE_COMMON_PORT']))
          server.bind(('127.0.0.1', port))
          server.listen()
          started[index].set()
          self.wait_for(started[1 - index])
          self.wait_for(release)
      return action

    cases = [EnvironmentCase('test-net-port', listener(0)),
             EnvironmentCase('test-http-port', listener(1))]
    progress = SchedulerProgress(cases)
    with mock.patch.dict(os.environ, {'NODE_COMMON_PORT': str(base)}):
      with self.running(progress, release=[release]) as complete:
        for event in started:
          self.wait_for(event)
        release.set()
        self.assertTrue(complete()['allPassed'])
    ports = [int(case.environments[0]['NODE_COMMON_PORT']) for case in cases]
    self.assertEqual(sorted(ports), [base, base + runner.SEQUENTIAL_PORT_RANGE])
    self.assertTrue(all(env['EXAMPLE'] == 'kept' for case in cases for env in case.environments))

  def test_port_ranges_preserve_retries_repeats_and_other_suites(self):
    def retry(case, env):
      if case.calls == 1:
        return runner.CommandOutput(1, False, '', '')

    retried = EnvironmentCase('test-net-retry', retry)
    retried.outcomes.add(runner.FLAKY)
    repeated = copy.deepcopy(retried)
    parallel = EnvironmentCase('test-parallel', suite='parallel', parallel=True)
    serial = EnvironmentCase('test-serial', suite='pummel')
    progress = SchedulerProgress([parallel, serial, retried, repeated], runner.KEEP_RETRYING)
    with mock.patch.dict(os.environ, {'NODE_COMMON_PORT': '20000'}), \
         contextlib.redirect_stdout(io.StringIO()):
      self.assertTrue(progress.Run(4)['allPassed'])
    for case in [retried, repeated]:
      self.assertEqual(case.calls, 2)
      self.assertEqual([env['NODE_COMMON_PORT'] for env in case.environments],
                       [str(20000 + case.thread_id * runner.SEQUENTIAL_PORT_RANGE)] * 2)
      self.assertTrue(all(env['TEST_PARALLEL'] == '0' for env in case.environments))
    for case in [parallel, serial]:
      self.assertNotIn('NODE_COMMON_PORT', case.environments[0])

  def test_single_worker_preserves_port_environment(self):
    case = EnvironmentCase('test-net-port')
    with mock.patch.dict(os.environ, {'NODE_COMMON_PORT': 'invalid'}), \
         mock.patch.object(runner.threading, 'Thread') as thread:
      self.assertTrue(SchedulerProgress([case]).Run(1)['allPassed'])
      thread.assert_not_called()
    self.assertNotIn('NODE_COMMON_PORT', case.environments[0])

  def test_port_range_defaults_and_valid_boundary(self):
    for configured, expected in [('', runner.DEFAULT_COMMON_PORT),
                                 ('0', runner.DEFAULT_COMMON_PORT),
                                 ('65136', 65136)]:
      with self.subTest(configured=configured), \
           mock.patch.dict(os.environ, {'NODE_COMMON_PORT': configured}):
        case = EnvironmentCase('test-net-port')
        self.assertTrue(SchedulerProgress([case]).Run(4)['allPassed'])
        self.assertEqual(case.environments[0]['NODE_COMMON_PORT'],
                         str(expected + case.thread_id * runner.SEQUENTIAL_PORT_RANGE))

  def test_invalid_port_ranges_fail_before_starting_any_tests(self):
    for base in ['invalid', '-1', '65137']:
      with self.subTest(base=base), mock.patch.dict(os.environ, {'NODE_COMMON_PORT': base}), \
           mock.patch.object(runner.threading, 'Thread') as thread:
        cases = [EnvironmentCase('test-net-port'),
                 EnvironmentCase('test-parallel', suite='parallel', parallel=True)]
        progress = SchedulerProgress(cases)
        with self.assertRaises(runner.PortRangeError):
          progress.Run(4)
        thread.assert_not_called()
        self.assertEqual([case.calls for case in cases], [0, 0])
        self.assertTrue(progress.shutdown_event.is_set())

    with mock.patch.dict(os.environ, {'NODE_COMMON_PORT': 'invalid'}):
      case = EnvironmentCase('test-serial', suite='pummel')
      self.assertTrue(SchedulerProgress([case]).Run(4)['allPassed'])

  def test_failure_report_includes_assigned_port(self):
    def fail(case, env):
      return runner.CommandOutput(1, False, '', '')

    case = EnvironmentCase('test-net-failed', fail)
    with mock.patch.dict(os.environ, {'NODE_COMMON_PORT': '20000'}):
      progress = SchedulerProgress([case])
      self.assertFalse(progress.Run(2)['allPassed'])
    self.assertIn('Environment: NODE_COMMON_PORT=%d' % case.common_port,
                  progress.GetFailureOutput(progress.failed[0]))

    failure = progress.failed[0]
    for indicator in [runner.MonochromeProgressIndicator, runner.ColorProgressIndicator]:
      with self.subTest(indicator=indicator.__name__), \
           contextlib.redirect_stdout(io.StringIO()) as report:
        indicator([case], runner.RUN, 0).HasRun(failure)
        self.assertIn('Environment: NODE_COMMON_PORT=%d' % case.common_port, report.getvalue())

    with mock.patch.object(runner.logger, 'info') as log:
      tap = runner.TapProgressIndicator([case], runner.RUN, 0)
      tap.Starting()
      tap.HasRun(failure)
      self.assertIn(mock.call('  environment: {NODE_COMMON_PORT: %d}', case.common_port),
                    log.call_args_list)

  def check_retries(self, measure_flakiness):
    retry_started = threading.Event()
    other_reported = threading.Event()
    retry_finished = threading.Event()

    def retry(case):
      if case.calls == 1:
        return runner.CommandOutput(1, False, '', '')
      retry_started.set()
      self.wait_for(other_reported)
      if case.calls == (3 if measure_flakiness else 2):
        retry_finished.set()

    def follower(case):
      self.assertTrue(retry_finished.is_set())

    def other(case):
      self.wait_for(retry_started)

    retried = SchedulerCase('test-net-retried', retry)
    if not measure_flakiness:
      retried.outcomes.add(runner.FLAKY)
    unrelated = SchedulerCase('test-http-unrelated', other)
    progress = SchedulerProgress(
        [retried, SchedulerCase('test-net-follower', follower), unrelated],
        flaky_tests_mode=runner.RUN if measure_flakiness else runner.KEEP_RETRYING,
        measure_flakiness=measure_flakiness, reported={unrelated: other_reported})
    with contextlib.redirect_stdout(io.StringIO()):
      with self.running(progress, release=[other_reported]) as complete:
        result = complete()
    self.assertEqual(retried.calls, 3 if measure_flakiness else 2)
    self.assertEqual(result['allPassed'], not bool(measure_flakiness))
    self.assertEqual(progress.remaining, 0)

  def test_flaky_retry_retains_gate_and_allows_other_progress(self):
    self.check_retries(0)

  def test_flakiness_measurement_retains_gate_and_allows_other_progress(self):
    self.check_retries(2)

  def test_failed_crashed_and_timed_out_cases_release_subsystem(self):
    for exit_code, timed_out, crashed in [(1, False, 0), (-9, False, 1), (-15, True, 0)]:
      with self.subTest(exit_code=exit_code, timed_out=timed_out):
        def fail(case):
          return runner.CommandOutput(exit_code, timed_out, '', '')

        cases = [SchedulerCase('test-net-failed', fail),
                 SchedulerCase('test-net-following'), SchedulerCase('test-http-other')]
        progress = SchedulerProgress(cases)
        with mock.patch.object(runner.utils, 'IsWindows', return_value=False):
          result = progress.Run(2)
        self.assertFalse(result['allPassed'])
        self.assertEqual(len(result['failed']), 1)
        self.assertEqual(progress.crashed, crashed)
        self.assertEqual(progress.remaining, 0)
        self.assertEqual([case.calls for case in cases], [1, 1, 1])

  def check_worker_error(self, error):
    worker_started = threading.Event()
    worker_failed = threading.Event()
    waiter_started = threading.Event()
    release = threading.Event()

    def action(case):
      if case.thread_id:
        worker_started.set()
        self.wait_for(release)
        worker_failed.set()
        raise type(error)(str(error))
      self.wait_for(worker_failed)

    progress = SchedulerProgress([SchedulerCase('test-net-first', action),
                                  SchedulerCase('test-net-following'),
                                  SchedulerCase('test-http-first', action),
                                  SchedulerCase('test-http-following')])
    original_wait = progress.condition.wait

    def wait(*args, **kwargs):
      waiter_started.set()
      return original_wait(*args, **kwargs)

    with mock.patch.object(progress.condition, 'wait', side_effect=wait):
      with self.running(progress, tasks=3, release=[release, worker_failed]) as complete:
        self.wait_for(worker_started)
        self.wait_for(waiter_started)
        release.set()
        if isinstance(error, IOError):
          self.assertFalse(complete()['allPassed'])
        else:
          with self.assertRaisesRegex(type(error), str(error)):
            complete()
    self.assertTrue(progress.shutdown_event.is_set())
    self.assertFalse(progress.running_subsystems)

  def test_shutdown_wakes_same_subsystem_waiters(self):
    first_started = threading.Event()
    waiter_started = threading.Event()
    release = threading.Event()

    def first(case):
      first_started.set()
      self.wait_for(release)

    cases = [SchedulerCase('test-net-first', first), SchedulerCase('test-net-following')]
    progress = SchedulerProgress(cases)
    original_wait = progress.condition.wait

    def wait(*args, **kwargs):
      waiter_started.set()
      return original_wait(*args, **kwargs)

    with mock.patch.object(progress.condition, 'wait', side_effect=wait):
      with self.running(progress, release=[release]) as complete:
        self.wait_for(first_started)
        self.wait_for(waiter_started)
        progress.Shutdown()
        release.set()
        self.assertFalse(complete()['allPassed'])
    self.assertEqual([case.calls for case in cases], [1, 0])
    self.assertFalse(progress.running_subsystems)

  def test_interrupt_during_join_waits_for_worker_cleanup(self):
    for interrupt in [KeyboardInterrupt, SystemExit]:
      with self.subTest(interrupt=interrupt.__name__):
        worker_started = threading.Event()
        interrupted = threading.Event()
        joined_again = threading.Event()
        release = threading.Event()
        done = threading.Event()
        workers = []

        def action(case):
          workers.append(threading.current_thread())
          worker_started.set()
          self.wait_for(release)

        progress = SchedulerProgress([SchedulerCase('test-net-first', action)])
        original_run_single = progress.RunSingle
        original_join = threading.Thread.join
        original_is_alive = threading.Thread.is_alive

        def run_single(queue, thread_id):
          if thread_id == 0 and queue is progress.sequential_queue:
            self.wait_for(worker_started)
          original_run_single(queue, thread_id)

        def join(thread, timeout=None):
          if thread in workers:
            if not interrupted.is_set():
              interrupted.set()
              raise interrupt()
            joined_again.set()
          original_join(thread, timeout)

        def is_alive(thread):
          # CPython 3.12 can report stopped after SIGINT interrupts join.
          if thread in workers and interrupted.is_set():
            return False
          return original_is_alive(thread)

        with mock.patch.object(progress, 'RunSingle', side_effect=run_single), \
             mock.patch.object(progress, 'Done', side_effect=done.set), \
             mock.patch.object(threading.Thread, 'join', join), \
             mock.patch.object(threading.Thread, 'is_alive', is_alive):
          with self.running(progress, release=[release]) as complete:
            self.wait_for(interrupted)
            self.wait_for(joined_again)
            self.assertFalse(done.is_set())
            self.assertEqual(progress.running_subsystems, {'net'})
            release.set()
            self.assertFalse(complete()['allPassed'])
        self.assertTrue(done.is_set())
        self.assertFalse(progress.running_subsystems)

  def test_worker_start_failure_does_not_wait_for_unstarted_thread(self):
    case = SchedulerCase('test-net-first')
    progress = SchedulerProgress([case])
    with mock.patch.object(threading.Thread, 'start', side_effect=RuntimeError('cannot start worker')):
      with self.assertRaisesRegex(RuntimeError, 'cannot start worker'):
        progress.Run(2)
    self.assertEqual(case.calls, 0)
    self.assertTrue(progress.shutdown_event.is_set())
    self.assertFalse(progress.running_subsystems)

  def test_worker_ioerror_aborts_run(self):
    self.check_worker_error(IOError('scheduler fixture interrupted'))

  def test_worker_exception_is_propagated_to_caller(self):
    self.check_worker_error(RuntimeError('scheduler fixture failed'))

  def test_progress_callback_exceptions_release_subsystem(self):
    for callback in ['AboutToRun', 'HasRun']:
      with self.subTest(callback=callback):
        progress = SchedulerProgress([SchedulerCase('test-net-first'),
                                      SchedulerCase('test-net-following')])
        with mock.patch.object(progress, callback, side_effect=RuntimeError('reporter failed')):
          with self.running(progress) as complete:
            with self.assertRaisesRegex(RuntimeError, 'reporter failed'):
              complete()
        self.assertFalse(progress.running_subsystems)


if __name__ == '__main__':
  unittest.main()
