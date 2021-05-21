#!/usr/bin/env python2.7
#
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from appx.util import appx_exe
import appx.util
import os
import subprocess
import unittest
import zipfile

class TestValidZIP(unittest.TestCase):
    '''
    Ensures the appx tool creates valid ZIP files.
    '''

    def test_unsigned_zip(self):
        with appx.util.temp_dir() as d:
            with open(os.path.join(d, 'README.txt'), 'wb') as readme:
                readme.write('This is a test file.\n')
            subprocess.check_call([appx_exe(),
                                   '-o', os.path.join(d, 'test.appx'),
                                   os.path.join(d, 'README.txt')])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertIsNone(zip.testzip())

    def test_signed_zip(self):
        with appx.util.temp_dir() as d:
            with open(os.path.join(d, 'README.txt'), 'wb') as readme:
                readme.write('This is a test file.\n')
            subprocess.check_call([appx_exe(),
                                   '-o', os.path.join(d, 'test.appx'),
                                   '-c', appx.util.test_key_path(),
                                   os.path.join(d, 'README.txt')])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertIsNone(zip.testzip())

    def test_unsigned_compressed_zip(self):
        with appx.util.temp_dir() as d:
            with open(os.path.join(d, 'README.txt'), 'wb') as readme:
                readme.write('This is a test file.\n')
            subprocess.check_call([appx_exe(),
                                   '-o', os.path.join(d, 'test.appx'),
                                   '-9',
                                   os.path.join(d, 'README.txt')])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertIsNone(zip.testzip())

    def test_signed_compressed_zip(self):
        with appx.util.temp_dir() as d:
            with open(os.path.join(d, 'README.txt'), 'wb') as readme:
                readme.write('This is a test file.\n')
            subprocess.check_call([appx_exe(),
                                   '-o', os.path.join(d, 'test.appx'),
                                   '-9',
                                   '-c', appx.util.test_key_path(),
                                   os.path.join(d, 'README.txt')])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertIsNone(zip.testzip())

if __name__ == '__main__':
    unittest.main()
