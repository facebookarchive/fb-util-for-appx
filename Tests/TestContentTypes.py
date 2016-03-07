#!/usr/bin/env python2.7
#
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
#

from appx.util import appx_exe
from xml.etree import ElementTree
import appx.util
import os
import re
import subprocess
import unittest
import zipfile

class TestContentTypes(unittest.TestCase):
    '''
    Ensures the content types XML gets generated correctly
    '''

    _default_content_type = 'application/octet-stream'

    def _get_content_types_xml(self, filename):
        with appx.util.temp_dir() as d:
            file_path = os.path.join(d, filename)
            with open(file_path, 'w') as test_file:
                pass # os.mknod requires super-user on OS X
            output_appx = os.path.join(d, 'test.appx')
            subprocess.check_call([appx_exe(),
                                   '-o', output_appx,
                                   file_path])
            with zipfile.ZipFile(output_appx) as test_appx:
                content_types_text = test_appx.read('[Content_Types].xml')
                # XML namespaces are a pain to deal with
                content_types_text = re.sub(' xmlns="[^"]+"', '',
                                            content_types_text, count=1)
                content_types_xml = ElementTree.fromstring(content_types_text)
                return content_types_xml

    def test_with_extension(self):
        content_types_xml = self._get_content_types_xml('foo.txt')
        extension_element = \
            content_types_xml.find('.//Default[@Extension="txt"]')
        self.assertIsNotNone(extension_element)
        self.assertEqual(self._default_content_type,
                         extension_element.get('ContentType'))

    def test_without_extension(self):
        content_types_xml = self._get_content_types_xml('foo')
        override_element = \
            content_types_xml.find('.//Override[@PartName="/foo"]')
        self.assertIsNotNone(override_element)
        self.assertEqual(self._default_content_type,
                         override_element.get('ContentType'))

    def test_escaping(self):
        content_types_xml = self._get_content_types_xml('foo bar.bar baz')
        extension_element = \
            content_types_xml.find('.//Default[@Extension="bar%20baz"]')
        self.assertIsNotNone(extension_element)
        self.assertEqual(self._default_content_type,
                         extension_element.get('ContentType'))

if __name__ == '__main__':
    unittest.main()
