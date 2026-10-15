import importlib.util
import os
import sys
import unittest
from os import path

REPO_ROOT = path.abspath(path.join(path.dirname(__file__), '..', '..'))

# The script name contains a hyphen so it cannot be imported directly.
_spec = importlib.util.spec_from_file_location(
  'generate_sbom',
  path.join(REPO_ROOT, 'tools', 'generate-sbom.py'),
)
sbom = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sbom)


class TestDepCatalog(unittest.TestCase):
  """Validate the DEP_CATALOG entries are structurally correct."""

  required_fields = (
    'name', 'description', 'purl_type', 'purl_namespace', 'purl_name',
    'component_type', 'default_scope', 'config_use_flag',
    'config_shared_flag', 'extra_conditions', 'version_fn',
  )

  def test_all_entries_have_required_fields(self):
    for dep in sbom.DEP_CATALOG:
      for field in self.required_fields:
        self.assertIn(field, dep,
          'DEP_CATALOG entry %r is missing field %r' % (dep.get('name'), field))

  def test_version_fn_is_callable(self):
    for dep in sbom.DEP_CATALOG:
      self.assertTrue(callable(dep['version_fn']),
        'version_fn for %r must be callable' % dep['name'])

  def test_default_scope_values(self):
    valid_scopes = {'required', 'optional', 'excluded'}
    for dep in sbom.DEP_CATALOG:
      self.assertIn(dep['default_scope'], valid_scopes,
        'invalid default_scope %r for %r' % (dep['default_scope'], dep['name']))

  def test_component_type_values(self):
    valid_types = {'library', 'framework'}
    for dep in sbom.DEP_CATALOG:
      self.assertIn(dep['component_type'], valid_types,
        'invalid component_type %r for %r' % (dep['component_type'], dep['name']))

  def test_no_duplicate_names(self):
    names = [dep['name'] for dep in sbom.DEP_CATALOG]
    self.assertEqual(len(names), len(set(names)),
      'DEP_CATALOG contains duplicate names: %r' %
      [n for n in names if names.count(n) > 1])

  def test_deps_dir_coverage(self):
    """Every first-level subdirectory of deps/ should have a DEP_CATALOG entry
    (or be a known artifact like icu-tmp)."""
    deps_dir = path.join(REPO_ROOT, 'deps')
    if not path.isdir(deps_dir):
      self.skipTest('deps/ directory not present')
    # Directories that are not independent deps requiring their own entry
    known_non_deps = {
      'icu-tmp',    # temporary download target, not a dep itself
      'icu-small',  # bundled copy of ICU, covered by the 'icu' entry
    }
    catalog_names = {dep['name'] for dep in sbom.DEP_CATALOG}
    # Normalise: dep dir names may differ from catalog names (e.g. 'uv' -> 'libuv')
    dir_to_catalog = {
      'uv': 'libuv',
      'cares': 'c-ares',
      'histogram': 'hdr_histogram',
      'crates': 'v8-crates',
    }
    missing = []
    for entry in sorted(os.listdir(deps_dir)):
      if not path.isdir(path.join(deps_dir, entry)):
        continue
      if entry in known_non_deps:
        continue
      canonical = dir_to_catalog.get(entry, entry)
      if canonical not in catalog_names:
        missing.append(entry)
    self.assertEqual(missing, [],
      'deps/ subdirectories with no DEP_CATALOG entry: %r' % missing)


class TestVersionExtractors(unittest.TestCase):
  """Smoke-test version extractors against real files in the repo."""

  def _skip_if_missing(self, *parts):
    p = path.join(REPO_ROOT, *parts)
    if not path.exists(p):
      self.skipTest('file not present: %s' % p)
    return p

  def test_version_from_package_json_npm(self):
    p = self._skip_if_missing('deps', 'npm', 'package.json')
    ver = sbom.version_from_package_json(p)
    self.assertNotEqual(ver, 'unknown')
    self.assertRegex(ver, r'^\d+\.\d+\.\d+')

  def test_version_from_c_macros_libuv(self):
    p = self._skip_if_missing('deps', 'uv', 'include', 'uv', 'version.h')
    ver = sbom.version_from_c_macros(
      p, 'UV_VERSION_MAJOR', 'UV_VERSION_MINOR', 'UV_VERSION_PATCH')
    self.assertNotEqual(ver, 'unknown')
    self.assertRegex(ver, r'^\d+\.\d+\.\d+')

  def test_version_from_string_macro_openssl(self):
    ver = sbom.version_from_openssl(sbom.REPO_ROOT)
    self.assertNotEqual(ver, 'unknown')
    self.assertRegex(ver, r'^\d+\.\d+\.\d+')

  def test_version_from_cmake_zlib(self):
    p = self._skip_if_missing('deps', 'zlib', 'CMakeLists.txt')
    ver = sbom.version_from_cmake(p)
    self.assertNotEqual(ver, 'unknown')
    self.assertRegex(ver, r'^\d+\.')

  def test_version_from_icu_dep(self):
    p = self._skip_if_missing('tools', 'icu', 'current_ver.dep')
    ver = sbom.version_from_icu_dep(p)
    self.assertNotEqual(ver, 'unknown')
    self.assertRegex(ver, r'^\d+\.')

  def test_version_from_plain_file_perfetto(self):
    p = self._skip_if_missing('deps', 'perfetto', 'VERSION')
    ver = sbom.version_from_plain_file(p)
    self.assertNotEqual(ver, 'unknown')
    self.assertTrue(len(ver) > 0)


class TestScopeResolution(unittest.TestCase):
  """Verify _resolve_scope() interprets config_vars correctly."""

  def _dep(self, **kwargs):
    base = {
      'default_scope': 'required',
      'config_use_flag': None,
      'config_shared_flag': None,
      'extra_conditions': [],
    }
    base.update(kwargs)
    return base

  def test_always_on_dep_is_required(self):
    dep = self._dep()
    self.assertEqual(sbom._resolve_scope(dep, {}), 'required')

  def test_use_flag_false_gives_excluded(self):
    dep = self._dep(config_use_flag='node_use_openssl')
    self.assertEqual(
      sbom._resolve_scope(dep, {'node_use_openssl': 'false'}), 'excluded')

  def test_use_flag_true_keeps_required(self):
    dep = self._dep(config_use_flag='node_use_openssl')
    self.assertEqual(
      sbom._resolve_scope(dep, {'node_use_openssl': 'true'}), 'required')

  def test_shared_flag_true_gives_excluded(self):
    dep = self._dep(config_shared_flag='node_shared_zlib')
    self.assertEqual(
      sbom._resolve_scope(dep, {'node_shared_zlib': 'true'}), 'excluded')

  def test_shared_flag_false_keeps_required(self):
    dep = self._dep(config_shared_flag='node_shared_zlib')
    self.assertEqual(
      sbom._resolve_scope(dep, {'node_shared_zlib': 'false'}), 'required')

  def test_default_excluded_stays_excluded(self):
    dep = self._dep(default_scope='excluded')
    self.assertEqual(sbom._resolve_scope(dep, {}), 'excluded')

  def test_extra_condition_excludes(self):
    dep = self._dep(
      config_use_flag='node_use_quic',
      extra_conditions=[('node_shared_openssl', 'true')],
    )
    config = {'node_use_quic': 'true', 'node_shared_openssl': 'true'}
    self.assertEqual(sbom._resolve_scope(dep, config), 'excluded')

  def test_extra_condition_does_not_exclude_when_not_matched(self):
    dep = self._dep(
      config_use_flag='node_use_quic',
      extra_conditions=[('node_shared_openssl', 'true')],
      default_scope='optional',
    )
    config = {'node_use_quic': 'true', 'node_shared_openssl': 'false'}
    self.assertEqual(sbom._resolve_scope(dep, config), 'optional')


class TestPurlBuilding(unittest.TestCase):

  def _dep(self, purl_type, purl_namespace, purl_name):
    return {
      'purl_type': purl_type,
      'purl_namespace': purl_namespace,
      'purl_name': purl_name,
    }

  def test_with_namespace(self):
    dep = self._dep('github', 'nodejs', 'node')
    self.assertEqual(sbom._build_purl(dep, '1.0.0'),
                     'pkg:github/nodejs/node@1.0.0')

  def test_without_namespace(self):
    dep = self._dep('npm', '', 'npm')
    self.assertEqual(sbom._build_purl(dep, '10.0.0'), 'pkg:npm/npm@10.0.0')


class TestBuildSbom(unittest.TestCase):

  def _make_component(self, name, version, scope='required'):
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
    components = [self._make_component('zlib', '1.3.0')]
    doc = sbom.build_sbom('27.0.0', components, [])
    self.assertEqual(doc['bomFormat'], 'CycloneDX')
    self.assertEqual(doc['specVersion'], '1.5')
    self.assertTrue(doc['serialNumber'].startswith('urn:uuid:'))
    self.assertEqual(doc['metadata']['component']['name'], 'nodejs')
    self.assertEqual(doc['metadata']['component']['version'], '27.0.0')
    self.assertEqual(len(doc['components']), 1)

  def test_excluded_not_in_root_depends_on(self):
    components = [
      self._make_component('zlib', '1.3.0', scope='required'),
      self._make_component('googletest', 'unknown', scope='excluded'),
    ]
    doc = sbom.build_sbom('27.0.0', components, [])
    root_deps = doc['dependencies'][0]
    self.assertEqual(root_deps['ref'], 'nodejs@27.0.0')
    self.assertIn('zlib@1.3.0', root_deps['dependsOn'])
    self.assertNotIn('googletest@unknown', root_deps['dependsOn'])

  def test_serial_number_unique(self):
    components = []
    doc1 = sbom.build_sbom('27.0.0', components, [])
    doc2 = sbom.build_sbom('27.0.0', components, [])
    self.assertNotEqual(doc1['serialNumber'], doc2['serialNumber'])


if __name__ == '__main__':
  unittest.main()
