import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock
from os import path

ROOT = path.abspath(path.join(path.dirname(__file__), '..', '..'))
sys.path.insert(0, path.join(ROOT, 'tools', 'sbom'))
import generate_sbom as sbom


class TestDepCatalog(unittest.TestCase):
  """Validate that the DEP_CATALOG entries are structurally correct."""

  def test_all_entries_have_required_fields(self):
    for dep in sbom.DEP_CATALOG:
      for field in ('name', 'description', 'purl_base', 'version_fn'):
        self.assertIn(field, dep,
          'DEP_CATALOG entry %r is missing field %r' % (dep.get('name'), field))

  def test_version_fn_is_callable(self):
    for dep in sbom.DEP_CATALOG:
      self.assertTrue(callable(dep['version_fn']),
        'version_fn for %r must be callable' % dep['name'])

  def test_scope_values(self):
    valid_scopes = {'required', 'optional', 'excluded'}
    for dep in sbom.DEP_CATALOG:
      scope = dep.get('scope', 'required')
      self.assertIn(scope, valid_scopes,
        'invalid scope %r for %r' % (scope, dep['name']))

  def test_component_type_values(self):
    valid_types = {'library', 'framework'}
    for dep in sbom.DEP_CATALOG:
      component_type = dep.get('component_type', 'library')
      self.assertIn(component_type, valid_types,
        'invalid component_type %r for %r' % (component_type, dep['name']))

  def test_purl_base_has_no_version(self):
    for dep in sbom.DEP_CATALOG:
      self.assertTrue(dep['purl_base'].startswith('pkg:'),
        'purl_base for %r must start with "pkg:"' % dep['name'])
      self.assertNotIn('@', dep['purl_base'],
        'purl_base for %r must not include a version' % dep['name'])

  def test_no_duplicate_names(self):
    names = [dep['name'] for dep in sbom.DEP_CATALOG]
    self.assertEqual(len(names), len(set(names)),
      'DEP_CATALOG contains duplicate names: %r' %
      [n for n in names if names.count(n) > 1])

  def test_deps_dir_coverage(self):
    """Every first-level subdirectory of deps/ needs a DEP_CATALOG entry."""
    deps_dir = path.join(ROOT, 'deps')
    if not path.isdir(deps_dir):
      self.skipTest('deps/ directory not present')
    # Directories that are not dependencies in their own right.
    known_non_deps = {
      'icu-tmp',    # temporary download target
      'icu-small',  # bundled subset of ICU, covered by the 'icu' entry
    }
    # Directory names that differ from the canonical dependency name.
    dir_to_catalog = {
      'uv': 'libuv',
      'cares': 'c-ares',
      'histogram': 'hdr_histogram',
      'crates': 'v8-crates',
    }
    catalog_names = {dep['name'] for dep in sbom.DEP_CATALOG}
    missing = []
    for entry in sorted(os.listdir(deps_dir)):
      if not path.isdir(path.join(deps_dir, entry)) or entry in known_non_deps:
        continue
      if dir_to_catalog.get(entry, entry) not in catalog_names:
        missing.append(entry)
    self.assertEqual(missing, [],
      'deps/ subdirectories with no DEP_CATALOG entry: %r' % missing)


class TestVersionExtractors(unittest.TestCase):
  """Smoke-test the version extractors against real files in the repo."""

  def _require(self, *parts):
    p = path.join(ROOT, *parts)
    if not path.exists(p):
      self.skipTest('file not present: %s' % p)
    return p

  def test_version_from_package_json(self):
    p = self._require('deps', 'npm', 'package.json')
    self.assertRegex(sbom.version_from_package_json(p), r'^\d+\.\d+\.\d+')

  def test_version_from_c_macros(self):
    p = self._require('deps', 'uv', 'include', 'uv', 'version.h')
    version = sbom.version_from_c_macros(
      p, 'UV_VERSION_MAJOR', 'UV_VERSION_MINOR', 'UV_VERSION_PATCH')
    self.assertRegex(version, r'^\d+\.\d+\.\d+$')

  def test_version_from_string_macro(self):
    p = self._require('deps', 'ada', 'ada.h')
    self.assertRegex(
      sbom.version_from_string_macro(p, 'ADA_VERSION'), r'^\d+\.')

  def test_version_from_cmake(self):
    p = self._require('deps', 'zlib', 'CMakeLists.txt')
    self.assertRegex(sbom.version_from_cmake(p), r'^\d+\.')

  def test_version_from_icu_dep(self):
    p = self._require('tools', 'icu', 'current_ver.dep')
    self.assertRegex(sbom.version_from_icu_dep(p), r'^\d+\.')

  def test_version_from_plain_file(self):
    p = self._require('deps', 'perfetto', 'VERSION')
    self.assertNotEqual(sbom.version_from_plain_file(p), sbom.UNKNOWN)

  def test_version_from_openssl(self):
    self.assertRegex(sbom.version_from_openssl(sbom.REPO_ROOT), r'^\d+\.\d+\.')

  def test_missing_file_is_unknown(self):
    missing = path.join(ROOT, 'deps', 'does-not-exist', 'version.h')
    # The extractors warn on stderr rather than raising.
    with contextlib.redirect_stderr(io.StringIO()) as stderr:
      version = sbom.version_from_string_macro(missing, 'FOO_VERSION')
    self.assertEqual(version, sbom.UNKNOWN)
    self.assertIn('cannot read', stderr.getvalue())

  def test_missing_macro_is_unknown(self):
    p = self._require('deps', 'ada', 'ada.h')
    with contextlib.redirect_stderr(io.StringIO()) as stderr:
      version = sbom.version_from_string_macro(p, 'NOT_A_MACRO')
    self.assertEqual(version, sbom.UNKNOWN)
    self.assertIn('no match', stderr.getvalue())


class TestResolveScope(unittest.TestCase):
  """Verify that resolve_scope() interprets config.gypi variables correctly."""

  def test_dependency_without_flags_is_required(self):
    self.assertEqual(sbom.resolve_scope({}, {}), 'required')

  def test_disabled_use_flag_is_excluded(self):
    dep = {'use_flag': 'node_use_sqlite'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_use_sqlite': 'false'}), 'excluded')

  def test_enabled_use_flag_keeps_scope(self):
    dep = {'use_flag': 'node_use_sqlite', 'scope': 'optional'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_use_sqlite': 'true'}), 'optional')

  def test_shared_flag_is_excluded(self):
    dep = {'shared_flag': 'node_shared_zlib'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_shared_zlib': 'true'}), 'excluded')

  def test_bundled_shared_flag_keeps_scope(self):
    dep = {'shared_flag': 'node_shared_zlib'}
    self.assertEqual(
      sbom.resolve_scope(dep, {'node_shared_zlib': 'false'}), 'required')

  def test_excluded_scope_stays_excluded(self):
    self.assertEqual(sbom.resolve_scope({'scope': 'excluded'}, {}), 'excluded')

  def test_excluded_when_matches(self):
    dep = {
      'scope': 'optional',
      'use_flag': 'node_use_quic',
      'excluded_when': [('node_shared_openssl', 'true')],
    }
    config_vars = {'node_use_quic': 'true', 'node_shared_openssl': 'true'}
    self.assertEqual(sbom.resolve_scope(dep, config_vars), 'excluded')

  def test_excluded_when_does_not_match(self):
    dep = {
      'scope': 'optional',
      'use_flag': 'node_use_quic',
      'excluded_when': [('node_shared_openssl', 'true')],
    }
    config_vars = {'node_use_quic': 'true', 'node_shared_openssl': 'false'}
    self.assertEqual(sbom.resolve_scope(dep, config_vars), 'optional')


class TestBuildComponent(unittest.TestCase):

  def test_fields(self):
    dep = {
      'name': 'zlib',
      'description': 'Compression library',
      'purl_base': 'pkg:github/madler/zlib',
    }
    component = sbom.build_component(dep, '1.3.0', 'required')
    self.assertEqual(component['bom-ref'], 'zlib@1.3.0')
    self.assertEqual(component['type'], 'library')
    self.assertEqual(component['version'], '1.3.0')
    self.assertEqual(component['scope'], 'required')
    self.assertEqual(component['purl'], 'pkg:github/madler/zlib@1.3.0')

  def test_component_type_override(self):
    dep = {
      'name': 'v8',
      'description': 'JavaScript engine',
      'purl_base': 'pkg:github/v8/v8',
      'component_type': 'framework',
    }
    self.assertEqual(
      sbom.build_component(dep, '13.0.0', 'required')['type'], 'framework')


class TestBuildSbom(unittest.TestCase):

  def _component(self, name, version, scope='required'):
    return {
      'bom-ref': '%s@%s' % (name, version),
      'type': 'library',
      'name': name,
      'version': version,
      'description': '',
      'scope': scope,
      'purl': 'pkg:generic/%s@%s' % (name, version),
    }

  def test_structure(self):
    doc = sbom.build_sbom('27.0.0', [self._component('zlib', '1.3.0')], [])
    self.assertEqual(doc['bomFormat'], 'CycloneDX')
    self.assertEqual(doc['specVersion'], '1.7')
    self.assertTrue(doc['serialNumber'].startswith('urn:uuid:'))
    self.assertEqual(doc['metadata']['component']['name'], 'nodejs')
    self.assertEqual(doc['metadata']['component']['version'], '27.0.0')
    self.assertEqual(len(doc['components']), 1)

  def test_root_depends_on_all_given_components(self):
    components = [
      self._component('zlib', '1.3.0'),
      self._component('openssl', '3.0.0', scope='optional'),
    ]
    doc = sbom.build_sbom('27.0.0', components, [])
    root = doc['dependencies'][0]
    self.assertEqual(root['ref'], 'nodejs@27.0.0')
    self.assertEqual(root['dependsOn'], ['zlib@1.3.0', 'openssl@3.0.0'])

  def test_generate_omits_excluded_components(self):
    # Excluded deps are filtered out by generate_sbom() before build_sbom().
    catalog = [
      {'name': 'zlib', 'description': '', 'purl_base': 'pkg:generic/zlib',
       'version_fn': lambda root: '1.3.0'},
      {'name': 'googletest', 'description': '',
       'purl_base': 'pkg:generic/googletest', 'scope': 'excluded',
       'version_fn': lambda root: 'unknown'},
    ]
    with mock.patch.object(sbom, 'DEP_CATALOG', catalog):
      doc = sbom.generate_sbom({}, '27.0.0')
    self.assertEqual([c['name'] for c in doc['components']], ['zlib'])
    root = doc['dependencies'][0]
    self.assertEqual(root['ref'], 'nodejs@27.0.0')
    self.assertEqual(root['dependsOn'], ['zlib@1.3.0'])

  def test_serial_number_is_unique(self):
    self.assertNotEqual(sbom.build_sbom('27.0.0', [], [])['serialNumber'],
                        sbom.build_sbom('27.0.0', [], [])['serialNumber'])

  def test_build_env_is_optional(self):
    # An empty build_env adds nothing to the document.
    without = sbom.build_sbom('27.0.0', [], [])
    with_env = sbom.build_sbom('27.0.0', [], [], {})
    for doc in (without, with_env):
      del doc['serialNumber']
      del doc['metadata']['timestamp']
    self.assertEqual(without, with_env)

  def test_compilers_are_emitted(self):
    build_env = {'compilers': [
      {'name': 'cc', 'command': 'ccache gcc',
       'output': 'gcc (GCC) 13.2.1 20240101'},
      {'name': 'rustc', 'command': 'rustc', 'output': sbom.NOT_AVAILABLE},
    ]}
    doc = sbom.build_sbom('27.0.0', [], [], build_env)
    props = {p['name']: p['value'] for p in doc['metadata']['properties']}
    self.assertEqual(props['nodejs:cc-version'], 'gcc (GCC) 13.2.1 20240101')
    self.assertEqual(props['nodejs:rustc-version'], 'N/A')
    components = doc['formulation'][0]['components']
    self.assertEqual([(c['name'], c['version']) for c in components],
                     [('cc', '13.2.1'), ('rustc', 'N/A')])
    self.assertEqual(components[0]['type'], 'application')
    self.assertIn('ccache gcc', components[0]['description'])

  def test_git_sha1_and_embedder_string_are_emitted(self):
    sha = 'a' * 40
    doc = sbom.build_sbom('27.0.0', [], [],
                          {'git_alg': 'SHA-1', 'git_sha': sha,
                           'v8_embedder_string': '-node.15'})
    props = {p['name']: p['value'] for p in doc['metadata']['properties']}
    self.assertEqual(props['nodejs:git-sha'], sha)
    self.assertEqual(props['nodejs:v8-embedder-string'], '-node.15')
    refs = doc['metadata']['component']['externalReferences']
    self.assertEqual(refs, [{
      'type': 'vcs',
      'url': 'https://github.com/nodejs/node',
      'hashes': [{'alg': 'SHA-1', 'content': sha}],
    }])

  def test_git_sha256_is_emitted_with_correct_alg(self):
    sha = 'b' * 64
    doc = sbom.build_sbom('27.0.0', [], [],
                          {'git_alg': 'SHA-256', 'git_sha': sha,
                           'v8_embedder_string': '-node.15'})
    refs = doc['metadata']['component']['externalReferences']
    self.assertEqual(refs, [{
      'type': 'vcs',
      'url': 'https://github.com/nodejs/node',
      'hashes': [{'alg': 'SHA-256', 'content': sha}],
    }])

  def test_unknown_git_sha_omits_vcs_reference(self):
    doc = sbom.build_sbom('27.0.0', [], [],
                          {'git_sha': sbom.NOT_AVAILABLE,
                           'git_alg': sbom.NOT_AVAILABLE,
                           'v8_embedder_string': sbom.NOT_AVAILABLE})
    props = {p['name']: p['value'] for p in doc['metadata']['properties']}
    self.assertNotIn('nodejs:git-sha', props)
    self.assertEqual(props['nodejs:v8-embedder-string'], 'N/A')
    self.assertNotIn('externalReferences', doc['metadata']['component'])

  def test_builder_is_emitted(self):
    doc = sbom.build_sbom('27.0.0', [], [], {'builder': 'build-host-1'})
    props = {p['name']: p['value'] for p in doc['metadata']['properties']}
    self.assertEqual(props['nodejs:builder'], 'build-host-1')

  def test_unknown_builder_is_emitted_as_not_available(self):
    doc = sbom.build_sbom('27.0.0', [], [], {'builder': sbom.NOT_AVAILABLE})
    props = {p['name']: p['value'] for p in doc['metadata']['properties']}
    self.assertEqual(props['nodejs:builder'], 'N/A')

  def test_no_git_properties_without_build_env(self):
    doc = sbom.build_sbom('27.0.0', [], [])
    names = [p['name'] for p in doc['metadata']['properties']]
    self.assertEqual(names, ['nodejs:bundled-lib-files'])
    self.assertNotIn('externalReferences', doc['metadata']['component'])

  def test_no_formulation_without_build_env(self):
    self.assertNotIn('formulation', sbom.build_sbom('27.0.0', [], []))

  def test_configure_command_is_a_formulation_property(self):
    doc = sbom.build_sbom('27.0.0', [], [],
                          {'configure_command': './configure --debug'})
    formula = doc['formulation'][0]
    self.assertNotIn('components', formula)
    self.assertEqual(formula['properties'],
                     [{'name': 'nodejs:configure-command',
                       'value': './configure --debug'}])

  def test_configure_command_alongside_compilers(self):
    doc = sbom.build_sbom('27.0.0', [], [], {
      'compilers': [{'name': 'cc', 'command': 'gcc', 'output': 'gcc 13.2.1'}],
      'configure_command': sbom.NOT_AVAILABLE,
    })
    formula = doc['formulation'][0]
    self.assertEqual(len(formula['components']), 1)
    self.assertEqual(formula['properties'][0]['value'], 'N/A')


class TestRun(unittest.TestCase):

  def test_returns_stdout(self):
    self.assertEqual(sbom._run([sys.executable, '-c', 'print("hi")']), 'hi')

  def test_missing_binary_returns_none(self):
    self.assertIsNone(sbom._run(['this-binary-does-not-exist-sbom']))

  def test_nonzero_exit_returns_none(self):
    self.assertIsNone(sbom._run([sys.executable, '-c', 'raise SystemExit(1)']))

  def test_empty_output_returns_none(self):
    self.assertIsNone(sbom._run([sys.executable, '-c', 'pass']))

  def test_timeout_returns_none(self):
    self.assertIsNone(sbom._run(
      [sys.executable, '-c', 'import time; time.sleep(5)'], timeout=0.1))


class TestGetCompilerVersion(unittest.TestCase):

  def test_set_and_working(self):
    with mock.patch.dict(os.environ, {'CC': 'gcc'}), \
         mock.patch.object(sbom, '_run',
                           return_value='gcc 13.2.1\nCopyright') as run:
      result = sbom.get_compiler_version('CC', 'cc')
    run.assert_called_once_with(['gcc', '--version'])
    self.assertEqual(result, {'command': 'gcc', 'output': 'gcc 13.2.1'})

  def test_unset_uses_default(self):
    env = {k: v for k, v in os.environ.items() if k != 'CC'}
    with mock.patch.dict(os.environ, env, clear=True), \
         mock.patch.object(sbom, '_run', return_value='cc 1.0') as run:
      result = sbom.get_compiler_version('CC', 'cc')
    run.assert_called_once_with(['cc', '--version'])
    self.assertEqual(result['command'], 'cc')
    self.assertEqual(result['output'], 'cc 1.0')

  def test_empty_variable_uses_default(self):
    with mock.patch.dict(os.environ, {'CC': ''}), \
         mock.patch.object(sbom, '_run', return_value='cc 1.0') as run:
      sbom.get_compiler_version('CC', 'cc')
    run.assert_called_once_with(['cc', '--version'])

  def test_missing_binary_is_not_available(self):
    with mock.patch.dict(os.environ, {'CC': 'nonexistent-cc'}), \
         mock.patch.object(sbom, '_run', return_value=None):
      result = sbom.get_compiler_version('CC', 'cc')
    self.assertEqual(result,
                     {'command': 'nonexistent-cc', 'output': sbom.NOT_AVAILABLE})

  def test_wrapper_and_flags_are_split(self):
    with mock.patch.dict(os.environ, {'CC': 'ccache gcc -m64'}), \
         mock.patch.object(sbom, '_run', return_value='gcc 13') as run:
      result = sbom.get_compiler_version('CC', 'cc')
    run.assert_called_once_with(['ccache', 'gcc', '-m64', '--version'])
    self.assertEqual(result['command'], 'ccache gcc -m64')

  def test_unbalanced_quotes_are_not_available(self):
    with mock.patch.dict(os.environ, {'CC': 'gcc "unterminated'}), \
         mock.patch.object(sbom, '_run') as run:
      result = sbom.get_compiler_version('CC', 'cc')
    run.assert_not_called()
    self.assertEqual(result['output'], sbom.NOT_AVAILABLE)


class TestGetGitSha(unittest.TestCase):

  def test_valid_sha1(self):
    sha = '0123456789abcdef' * 2 + '01234567'
    with mock.patch.object(sbom, '_run', return_value=sha) as run:
      self.assertEqual(sbom.get_git_sha('/some/root'), ('SHA-1', sha))
    run.assert_called_once_with(['git', '-C', '/some/root', 'rev-parse', 'HEAD'])

  def test_valid_sha256(self):
    sha = 'ab' * 32
    with mock.patch.object(sbom, '_run', return_value=sha):
      self.assertEqual(sbom.get_git_sha('.'), ('SHA-256', sha))

  def test_garbage_is_not_available(self):
    for output in ('HEAD', 'fatal: not a git repository', 'ABC' * 14,
                   'g' * 40, 'a' * 39, 'a' * 41, '0' * 40 + '\n' + '0' * 40):
      with mock.patch.object(sbom, '_run', return_value=output):
        self.assertEqual(sbom.get_git_sha('.'),
                         (sbom.NOT_AVAILABLE, sbom.NOT_AVAILABLE), output)

  def test_git_failure_is_not_available(self):
    with mock.patch.object(sbom, '_run', return_value=None):
      self.assertEqual(sbom.get_git_sha('.'),
                       (sbom.NOT_AVAILABLE, sbom.NOT_AVAILABLE))

  def test_outside_a_repository(self):
    with tempfile.TemporaryDirectory() as tmp:
      # Stop git searching parent directories for a repository.
      with mock.patch.dict(os.environ, {'GIT_CEILING_DIRECTORIES': tmp}):
        self.assertEqual(sbom.get_git_sha(tmp),
                         (sbom.NOT_AVAILABLE, sbom.NOT_AVAILABLE))


class TestGetV8EmbedderString(unittest.TestCase):

  def test_read_from_real_common_gypi(self):
    value = sbom.get_v8_embedder_string(sbom.REPO_ROOT)
    self.assertRegex(value, r'^-node\.\d+$')

  def _read(self, content):
    with tempfile.TemporaryDirectory() as tmp:
      with open(path.join(tmp, 'common.gypi'), 'w', encoding='utf-8') as f:
        f.write(content)
      with contextlib.redirect_stderr(io.StringIO()):
        return sbom.get_v8_embedder_string(tmp)

  def test_custom_value(self):
    self.assertEqual(self._read("{'v8_embedder_string': '-custom.2',}"),
                     '-custom.2')

  def test_missing_variable_is_not_available(self):
    self.assertEqual(self._read("{'other': 'x'}"), sbom.NOT_AVAILABLE)

  def test_empty_value_is_not_available(self):
    self.assertEqual(self._read("{'v8_embedder_string': '',}"),
                     sbom.NOT_AVAILABLE)

  def test_missing_file_is_not_available(self):
    with tempfile.TemporaryDirectory() as tmp:
      with contextlib.redirect_stderr(io.StringIO()):
        self.assertEqual(sbom.get_v8_embedder_string(tmp), sbom.NOT_AVAILABLE)


class TestGetConfigureCommand(unittest.TestCase):

  def _command_for(self, content):
    with tempfile.TemporaryDirectory() as tmp:
      if content is not None:
        with open(path.join(tmp, 'config.status'), 'w',
                  encoding='utf-8') as f:
          f.write(content)
      with contextlib.redirect_stderr(io.StringIO()):
        return sbom.get_configure_command(tmp)

  def test_typical_file(self):
    content = ("#!/bin/sh\nset -x\nexec ./configure --prefix=/opt/node "
               "--without-intl\n")
    self.assertEqual(self._command_for(content),
                     './configure --prefix=/opt/node --without-intl')

  def test_no_arguments(self):
    self.assertEqual(self._command_for('#!/bin/sh\nset -x\nexec ./configure \n'),
                     './configure')

  def test_quoted_arguments_with_spaces(self):
    content = ("#!/bin/sh\nset -x\nexec ./configure "
               "'--prefix=/opt/my node' --debug\n")
    self.assertEqual(self._command_for(content),
                     "./configure '--prefix=/opt/my node' --debug")

  def test_argument_containing_newline(self):
    content = "#!/bin/sh\nset -x\nexec ./configure 'a\nb' --debug\n"
    self.assertEqual(self._command_for(content),
                     "./configure 'a\nb' --debug")

  def test_missing_file_is_not_available(self):
    self.assertEqual(self._command_for(None), sbom.NOT_AVAILABLE)

  def test_malformed_content_is_not_available(self):
    for content in ('', 'garbage\n', '#!/bin/sh\nset -x\n',
                    '#!/bin/sh\nexec ./other --debug\n',
                    "#!/bin/sh\nexec ./configure 'unbalanced\n"):
      self.assertEqual(self._command_for(content), sbom.NOT_AVAILABLE,
                       repr(content))


class TestGetBuilderName(unittest.TestCase):

  def test_returns_hostname(self):
    with mock.patch('socket.gethostname', return_value='build-host-1'):
      self.assertEqual(sbom.get_builder_name(), 'build-host-1')

  def test_strips_whitespace(self):
    with mock.patch('socket.gethostname', return_value=' build-host-1\n'):
      self.assertEqual(sbom.get_builder_name(), 'build-host-1')

  def test_empty_hostname_is_not_available(self):
    for value in ('', '  '):
      with mock.patch('socket.gethostname', return_value=value):
        self.assertEqual(sbom.get_builder_name(), sbom.NOT_AVAILABLE)

  def test_error_is_not_available(self):
    with mock.patch('socket.gethostname', side_effect=OSError('boom')):
      self.assertEqual(sbom.get_builder_name(), sbom.NOT_AVAILABLE)


class TestParseVersion(unittest.TestCase):

  def test_extracts_dotted_version(self):
    self.assertEqual(sbom._parse_version('gcc (GCC) 13.2.1 2024'), '13.2.1')
    self.assertEqual(sbom._parse_version('rustc 1.85.0 (abc 2025-01-01)'),
                     '1.85.0')

  def test_falls_back_to_input(self):
    self.assertEqual(sbom._parse_version('mystery tool'), 'mystery tool')


class TestCollectBuildEnvironment(unittest.TestCase):

  def test_reports_configure_command(self):
    with tempfile.TemporaryDirectory() as tmp:
      with open(path.join(tmp, 'config.status'), 'w', encoding='utf-8') as f:
        f.write('#!/bin/sh\nset -x\nexec ./configure --debug\n')
      with contextlib.redirect_stderr(io.StringIO()):
        env = sbom.collect_build_environment(tmp, {})
    self.assertEqual(env['configure_command'], './configure --debug')

  def test_reports_all_tools(self):
    with mock.patch.object(sbom, '_run', return_value='tool 1.2.3'):
      env = sbom.collect_build_environment(sbom.REPO_ROOT, {})
    self.assertEqual([c['name'] for c in env['compilers']],
                     ['cc', 'cxx', 'rustc', 'cargo'])
    self.assertTrue(all(c['output'] == 'tool 1.2.3'
                        for c in env['compilers']))

  def test_reports_git_sha_and_embedder_string(self):
    sha = 'c' * 40
    with mock.patch.object(sbom, '_run', return_value=sha):
      env = sbom.collect_build_environment(sbom.REPO_ROOT, {})
    self.assertEqual(env['git_sha'], sha)
    self.assertEqual(env['git_alg'], 'SHA-1')
    self.assertRegex(env['v8_embedder_string'], r'^-node\.\d+$')

  def test_reports_builder(self):
    with mock.patch('socket.gethostname', return_value='build-host-1'):
      env = sbom.collect_build_environment(sbom.REPO_ROOT, {})
    self.assertEqual(env['builder'], 'build-host-1')

  def test_all_tools_missing(self):
    with mock.patch.object(sbom, '_run', return_value=None):
      env = sbom.collect_build_environment(sbom.REPO_ROOT, {})
    self.assertTrue(all(c['output'] == sbom.NOT_AVAILABLE
                        for c in env['compilers']))

  def test_platform_defaults(self):
    for plat, cc, cxx in (('darwin', 'cc', 'c++'),
                          ('win32', 'clang', 'clang'),
                          ('linux', 'gcc', 'g++')):
      env = {k: v for k, v in os.environ.items()
             if k not in ('CC', 'CXX')}
      with mock.patch.dict(os.environ, env, clear=True), \
           mock.patch.object(sys, 'platform', plat), \
           mock.patch.object(sbom, '_run', return_value='x 1.0'), \
           mock.patch('shutil.which', return_value='/usr/bin/cc'):
        result = sbom.collect_build_environment(sbom.REPO_ROOT, {})
      commands = {c['name']: c['command'] for c in result['compilers']}
      self.assertEqual((commands['cc'], commands['cxx']), (cc, cxx), plat)


class TestGetGlibcVersion(unittest.TestCase):

  def test_returns_version_on_linux(self):
    with mock.patch.object(sys, 'platform', 'linux'), \
         mock.patch('platform.libc_ver', return_value=('glibc', '2.34')):
      self.assertEqual(sbom.get_glibc_version(), '2.34')

  def test_non_linux_is_not_available(self):
    for plat in ('darwin', 'win32', 'freebsd7'):
      with mock.patch.object(sys, 'platform', plat):
        self.assertEqual(sbom.get_glibc_version(), sbom.NOT_AVAILABLE, plat)

  def test_musl_is_not_available(self):
    with mock.patch.object(sys, 'platform', 'linux'), \
         mock.patch('platform.libc_ver', return_value=('', '')):
      self.assertEqual(sbom.get_glibc_version(), sbom.NOT_AVAILABLE)

  def test_empty_version_is_not_available(self):
    with mock.patch.object(sys, 'platform', 'linux'), \
         mock.patch('platform.libc_ver', return_value=('glibc', '')):
      self.assertEqual(sbom.get_glibc_version(), sbom.NOT_AVAILABLE)


class TestGetCompilerPackageVersion(unittest.TestCase):

  def test_non_linux_is_not_available(self):
    for plat in ('darwin', 'win32'):
      with mock.patch.object(sys, 'platform', plat):
        self.assertEqual(
          sbom.get_compiler_package_version('/usr/bin/gcc'), sbom.NOT_AVAILABLE)

  def test_no_binary_path_is_not_available(self):
    with mock.patch.object(sys, 'platform', 'linux'):
      for bad in (None, ''):
        self.assertEqual(
          sbom.get_compiler_package_version(bad), sbom.NOT_AVAILABLE)

  def test_dpkg_hit(self):
    def fake_run(args, **kw):
      if args[:2] == ['dpkg', '-S']:
        return 'gcc-13: /usr/bin/gcc'
      if args[:2] == ['dpkg-query', '-W']:
        return '13.2.1-6ubuntu1'
      return None
    with mock.patch.object(sys, 'platform', 'linux'), \
         mock.patch.object(sbom, '_run', side_effect=fake_run):
      self.assertEqual(
        sbom.get_compiler_package_version('/usr/bin/gcc'), '13.2.1-6ubuntu1')

  def test_rpm_hit(self):
    def fake_run(args, **kw):
      if args[:2] == ['dpkg', '-S']:
        return None
      if args[:1] == ['rpm']:
        return '13.3.1-1.el9'
      return None
    with mock.patch.object(sys, 'platform', 'linux'), \
         mock.patch.object(sbom, '_run', side_effect=fake_run):
      self.assertEqual(
        sbom.get_compiler_package_version('/usr/bin/gcc'), '13.3.1-1.el9')

  def test_neither_package_manager_found(self):
    with mock.patch.object(sys, 'platform', 'linux'), \
         mock.patch.object(sbom, '_run', return_value=None):
      self.assertEqual(
        sbom.get_compiler_package_version('/usr/bin/gcc'), sbom.NOT_AVAILABLE)

  def test_dpkg_without_colon_falls_through_to_rpm(self):
    # malformed dpkg -S output → falls through to rpm
    def fake_run(args, **kw):
      if args[:2] == ['dpkg', '-S']:
        return 'no colon here'
      if args[:1] == ['rpm']:
        return '13.3.1-1.el9'
      return None
    with mock.patch.object(sys, 'platform', 'linux'), \
         mock.patch.object(sbom, '_run', side_effect=fake_run):
      self.assertEqual(
        sbom.get_compiler_package_version('/usr/bin/gcc'), '13.3.1-1.el9')


class TestBuildSbomGlibc(unittest.TestCase):
  """Verify that glibc and compiler package-version appear correctly in build_sbom."""

  def _compiler(self, name='cc', command='gcc', output='gcc 13.2.1',
                binary_path='/usr/bin/gcc'):
    return {'name': name, 'command': command, 'output': output,
            'binary_path': binary_path}

  def test_glibc_component_present(self):
    build_env = {
      'compilers': [],
      'glibc_version': '2.34',
    }
    doc = sbom.build_sbom('27.0.0', [], [], build_env)
    comps = doc['formulation'][0]['components']
    glibc = next(c for c in comps if c['name'] == 'glibc')
    self.assertEqual(glibc['version'], '2.34')
    self.assertEqual(glibc['bom-ref'], 'build-environment:glibc')
    self.assertEqual(glibc['type'], 'library')

  def test_glibc_not_available_not_emitted_as_component(self):
    build_env = {'glibc_version': sbom.NOT_AVAILABLE}
    doc = sbom.build_sbom('27.0.0', [], [], build_env)
    # N/A glibc should not produce a formulation at all (no other fields set).
    self.assertNotIn('formulation', doc)

  def test_compiler_package_version_property(self):
    def fake_pkg(path):
      return '13.2.1-6ubuntu1' if path else sbom.NOT_AVAILABLE
    with mock.patch.object(sbom, 'get_compiler_package_version',
                           side_effect=fake_pkg):
      build_env = {'compilers': [self._compiler()]}
      doc = sbom.build_sbom('27.0.0', [], [], build_env)
    comp = doc['formulation'][0]['components'][0]
    self.assertEqual(comp['properties'],
                     [{'name': 'nodejs:package-version',
                       'value': '13.2.1-6ubuntu1'}])

  def test_compiler_without_package_version_has_no_properties(self):
    with mock.patch.object(sbom, 'get_compiler_package_version',
                           return_value=sbom.NOT_AVAILABLE):
      build_env = {'compilers': [self._compiler(binary_path=None)]}
      doc = sbom.build_sbom('27.0.0', [], [], build_env)
    comp = doc['formulation'][0]['components'][0]
    self.assertNotIn('properties', comp)

  def test_glibc_alongside_compilers(self):
    with mock.patch.object(sbom, 'get_compiler_package_version',
                           return_value=sbom.NOT_AVAILABLE):
      build_env = {
        'compilers': [self._compiler()],
        'glibc_version': '2.38',
      }
      doc = sbom.build_sbom('27.0.0', [], [], build_env)
    names = [c['name'] for c in doc['formulation'][0]['components']]
    self.assertIn('cc', names)
    self.assertIn('glibc', names)
    # glibc comes last (after compilers)
    self.assertEqual(names[-1], 'glibc')


class TestCollectBuildEnvironmentGlibc(unittest.TestCase):

  def test_glibc_version_key_present(self):
    with mock.patch.object(sbom, '_run', return_value='tool 1.0'), \
         mock.patch.object(sbom, 'get_glibc_version', return_value='2.34'):
      env = sbom.collect_build_environment(sbom.REPO_ROOT, {})
    self.assertEqual(env['glibc_version'], '2.34')

  def test_binary_path_resolved_for_compilers(self):
    with mock.patch.object(sbom, '_run', return_value='gcc 13.2.1'), \
         mock.patch.object(sbom, 'get_glibc_version', return_value='2.34'), \
         mock.patch('shutil.which', return_value='/usr/bin/gcc'):
      env = sbom.collect_build_environment(sbom.REPO_ROOT, {})
    for c in env['compilers']:
      self.assertIn('binary_path', c)

  def test_binary_path_is_none_when_which_fails(self):
    with mock.patch.object(sbom, '_run', return_value='gcc 13.2.1'), \
         mock.patch.object(sbom, 'get_glibc_version', return_value=sbom.NOT_AVAILABLE), \
         mock.patch('shutil.which', return_value=None):
      env = sbom.collect_build_environment(sbom.REPO_ROOT, {})
    for c in env['compilers']:
      self.assertIsNone(c['binary_path'])


if __name__ == '__main__':
  unittest.main()
