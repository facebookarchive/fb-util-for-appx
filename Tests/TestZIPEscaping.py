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
import appx.util
import os
import subprocess
import unittest
import urllib
import zipfile

class TestZIPEscaping(unittest.TestCase):
    '''
    Ensures file names inside the ZIP are correctly URL-encoded.
    '''

    def _get_escaped_filename(self, filename):
        return urllib.quote(filename, safe='~/')

    def _create_appx_with_file(self, d, filename):
        file_path = os.path.join(d, filename)
        with open(file_path, 'w') as test_file:
            pass # os.mknod requires super-user on OS X
        output_appx = os.path.join(d, 'test.appx')
        subprocess.check_call([appx_exe(),
                               '-o', output_appx,
                               file_path])
        return output_appx

    def test_regular_file(self):
        with appx.util.temp_dir() as d:
            filename = 'foo.txt'
            appx_path = self._create_appx_with_file(d, filename)
            with zipfile.ZipFile(appx_path) as test_appx:
                self.assertIn(filename, test_appx.namelist())

    def test_special_characters(self):
        with appx.util.temp_dir() as d:
            filename = "~hello!world&%@'[foo]"
            appx_path = self._create_appx_with_file(d, filename)
            with zipfile.ZipFile(appx_path) as test_appx:
                escaped_filename = self._get_escaped_filename(filename)
                self.assertIn(escaped_filename, test_appx.namelist())

    def test_content_types(self):
        with appx.util.temp_dir() as d:
            appx_path = self._create_appx_with_file(d, 'ignore.txt')
            with zipfile.ZipFile(appx_path) as test_appx:
                self.assertIn('[Content_Types].xml', test_appx.namelist())

    def test_unicode_filename(self):
        with appx.util.temp_dir() as d:
            filename = u'\u00c0\u0800\U00010000'.encode('utf-8')
            appx_path = self._create_appx_with_file(d, filename)
            with zipfile.ZipFile(appx_path) as test_appx:
                escaped_filename = self._get_escaped_filename(filename)
                self.assertIn(escaped_filename, test_appx.namelist())

if __name__ == '__main__':
    unittest.main()
