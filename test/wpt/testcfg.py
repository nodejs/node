import json
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import testpy
import test


MANIFEST_PREFIX = 'NODE_TEST_WPT_MANIFEST:'


class WPTTestCase(testpy.SimpleTestCase):
  def __init__(self, path, file, arch, mode, context, config, group, serial):
    super(WPTTestCase, self).__init__(
        path, file, arch, mode, context, config, config.additional_flags)
    self.group = group
    self.parallel = not serial

  def GetName(self):
    return '/'.join(self.path)

  def GetReportingName(self, command):
    return self.GetName()

  def GetRunConfiguration(self):
    configuration = super(WPTTestCase, self).GetRunConfiguration()
    request = {
        'mode': 'run', 'source': self.group['source'], 'key': self.group['key'],
    }
    if 'variant' in self.group:
      request['variant'] = self.group['variant']
    configuration['envs']['NODE_TEST_WPT'] = json.dumps(request)
    return configuration


class WPTTestConfiguration(testpy.SimpleTestConfiguration):
  def __init__(self, context, root):
    super(WPTTestConfiguration, self).__init__(context, root, 'wpt')
    self.manifests = {}

  def _Discover(self, wrapper):
    key = (wrapper.file, wrapper.arch, wrapper.mode)
    if key in self.manifests:
      return self.manifests[key]
    configuration = wrapper.GetRunConfiguration()
    configuration['envs']['NODE_TEST_WPT'] = json.dumps({'mode': 'list'})
    # common/tmpdir resolves this path before Main creates the execution dir.
    configuration['envs']['NODE_TEST_DIR'] = os.path.abspath(self.root)
    output = test.Execute(
        self.context.processor(configuration['command']), self.context,
        self.context.GetTimeout(wrapper.mode), configuration['envs'])
    if output.exit_code != 0 or output.timed_out:
      raise RuntimeError('WPT discovery failed for %s:\n%s%s' % (
          wrapper.file, output.stdout, output.stderr))
    manifests = [line[len(MANIFEST_PREFIX):] for line in output.stdout.splitlines()
                 if line.startswith(MANIFEST_PREFIX)]
    if not manifests and test.skip_regex.search(output.stdout):
      self.manifests[key] = None
      return None
    try:
      if len(manifests) != 1:
        raise ValueError('expected exactly one WPT manifest')
      manifest = json.loads(manifests[0])
      if (manifest.get('version') != 1 or
          not isinstance(manifest.get('serial'), bool) or
          not isinstance(manifest.get('tests'), list) or
          any(not isinstance(group.get(field), str)
              for group in manifest['tests'] for field in ['source', 'key', 'id']) or
          any('variant' in group and not isinstance(group['variant'], str)
              for group in manifest['tests'])):
        raise ValueError('invalid WPT manifest')
    except (AttributeError, TypeError, ValueError) as error:
      raise RuntimeError('WPT discovery failed for %s: %s' % (
          wrapper.file, error)) from error
    self.manifests[key] = manifest
    return manifest

  def ListTests(self, current_path, path, arch, mode):
    wrappers = super(WPTTestConfiguration, self).ListTests(
        current_path, path[:2], arch, mode)
    selector = '/'.join(part.pattern for part in path[2:])
    pattern = re.escape(selector).replace(r'\*', '.*') + r'(?:/.*)?'
    result = []
    for wrapper in wrappers:
      manifest = self._Discover(wrapper)
      if manifest is None:
        result.append(wrapper)
        continue
      for group in manifest['tests']:
        case_path = wrapper.path + group['id'].split('/')
        # Query selectors are literal; a query-free path selects all its variants.
        if '?' in selector:
          selected = group['id'] == selector
        else:
          selected = not selector or re.fullmatch(pattern, group['id'].split('?')[0])
        if selected:
          result.append(WPTTestCase(case_path, wrapper.file, arch, mode,
                                    self.context, self, group, manifest['serial']))
    return result


def GetConfiguration(context, root):
  return WPTTestConfiguration(context, root)
