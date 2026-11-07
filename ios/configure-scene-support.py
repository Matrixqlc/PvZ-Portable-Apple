#!/usr/bin/env python3
"""Patch a generated iOS project in place while preserving all signing settings.

Run after CMake configuration. It adds only the scene bridge compilation unit
and the generated Info.plist scene configuration; it never reruns CMake.
"""
import hashlib
import json
from pathlib import Path
import plistlib
import re
import subprocess
import sys


def load_project(path):
    return json.loads(subprocess.check_output(
        ['/usr/bin/plutil', '-convert', 'json', '-o', '-', str(path)]))


def signing_settings(project):
    keys = ('CODE_SIGN_IDENTITY', 'CODE_SIGN_STYLE', 'DEVELOPMENT_TEAM',
            'PROVISIONING_PROFILE', 'PROVISIONING_PROFILE_SPECIFIER',
            'CODE_SIGN_ENTITLEMENTS', 'PRODUCT_BUNDLE_IDENTIFIER')
    return {key: {name: value.get('buildSettings', {}).get(name) for name in keys}
            for key, value in project['objects'].items()
            if value.get('isa') == 'XCBuildConfiguration'}


def insert_reference(text, object_id, field, reference):
    pattern = r'(?ms)^\t\t' + re.escape(object_id) + r'(?: /\*[^\n]*\*/)? = \{.*?^\t\t\};'
    match = re.search(pattern, text)
    if not match:
        raise ValueError('Cannot find project object ' + object_id)
    block = match.group()
    marker = field + ' = (\n'
    if block.count(marker) != 1:
        raise ValueError('Cannot find unique list ' + field)
    block = block.replace(marker, marker + '\t\t\t\t' + reference + ',\n', 1)
    return text[:match.start()] + block + text[match.end():]


def main():
    directory = Path(sys.argv[1]).resolve()
    source = Path(__file__).resolve().parent.parent
    path = directory / 'pvz-portable.xcodeproj/project.pbxproj'
    project = load_project(path)
    before = signing_settings(project)
    objects = project['objects']
    app = next(value for value in objects.values()
               if value.get('isa') == 'PBXNativeTarget'
               and value.get('name') == 'pvz-portable')
    phase = next(key for key in app['buildPhases']
                 if objects[key].get('isa') == 'PBXSourcesBuildPhase')
    reference = hashlib.sha1(b'pvz-single-scene-file-reference').hexdigest()[:24].upper()
    build_file = hashlib.sha1(b'pvz-single-scene-build-file').hexdigest()[:24].upper()
    text = path.read_text()
    bridge = source / 'ios/PvZSceneDelegate.m'
    if reference not in objects:
        assert build_file not in objects
        file_line = ('\t\t' + reference + ' /* PvZSceneDelegate.m */ = '
                     '{isa = PBXFileReference; lastKnownFileType = sourcecode.c.objc; '
                     'name = PvZSceneDelegate.m; path = ' + json.dumps(str(bridge))
                     + '; sourceTree = "<absolute>"; };\n')
        build_line = ('\t\t' + build_file + ' /* PvZSceneDelegate.m in Sources */ = '
                      '{isa = PBXBuildFile; fileRef = ' + reference
                      + ' /* PvZSceneDelegate.m */; settings = {COMPILER_FLAGS = "-fobjc-arc"; }; };\n')
        for section, line in [('PBXFileReference', file_line), ('PBXBuildFile', build_line)]:
            marker = '/* End ' + section + ' section */'
            assert text.count(marker) == 1
            text = text.replace(marker, line + marker, 1)
        main_group = objects[project['rootObject']]['mainGroup']
        text = insert_reference(text, main_group, 'children', reference + ' /* PvZSceneDelegate.m */')
        text = insert_reference(text, phase, 'files', build_file + ' /* PvZSceneDelegate.m in Sources */')
        path.write_text(text)
    after = load_project(path)
    if signing_settings(after) != before:
        raise ValueError('Signing configuration changed unexpectedly')
    assert after['objects'][reference]['path'] == str(bridge)
    assert build_file in after['objects'][phase]['files']
    scene_manifest = {
        'UIApplicationSupportsMultipleScenes': False,
        'UISceneConfigurations': {
            'UIWindowSceneSessionRoleApplication': [{
                'UISceneConfigurationName': 'PvZ Single Window',
                'UISceneClassName': 'UIWindowScene',
                'UISceneDelegateClassName': 'PVZSceneDelegate',
            }],
        },
    }
    plist_paths = {directory / 'Info.plist'}
    for configuration_id in objects[app['buildConfigurationList']]['buildConfigurations']:
        configured_path = Path(objects[configuration_id]['buildSettings']['INFOPLIST_FILE'])
        if not configured_path.is_absolute():
            configured_path = directory / configured_path
        configured_path.resolve().relative_to(directory)
        plist_paths.add(configured_path)
    for plist_path in plist_paths:
        data = plistlib.loads(plist_path.read_bytes())
        data['UIApplicationSceneManifest'] = scene_manifest
        data['UIViewControllerBasedStatusBarAppearance'] = True
        plist_path.write_bytes(plistlib.dumps(data, sort_keys=False))
    print('Scene bridge configured; all existing signing settings preserved.')


if __name__ == '__main__':
    main()
