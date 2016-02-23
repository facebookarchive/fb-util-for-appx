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
import zipfile

class TestInputs(unittest.TestCase):
    '''
    Ensures the appx tool creates ZIP files with the correct files inside.
    '''

    def test_flat_files(self):
        with appx.util.temp_dir() as d:
            with open(os.path.join(d, 'README.txt'), 'wb') as readme:
                readme.write('This is a test file.\n')
            os.mkdir(os.path.join(d, 'somedir'))
            with open(os.path.join(d, 'somedir', 'other_file.dll'), 'wb') \
                as other_file:
                other_file.write('MZ')
            subprocess.check_call([
                appx_exe(), '-o', os.path.join(d, 'test.appx'),
                os.path.join(d, 'README.txt'),
                os.path.join(d, 'somedir', 'other_file.dll')])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertIn('README.txt', zip.namelist())
                self.assertIn('other_file.dll', zip.namelist())
                self.assertNotIn('somedir/other_file.dll', zip.namelist())

    def test_directory(self):
        with appx.util.temp_dir() as d:
            with open(os.path.join(d, 'README.txt'), 'wb') as readme:
                readme.write('This is a test file.\n')
            os.mkdir(os.path.join(d, 'somedir'))
            with open(os.path.join(d, 'somedir', 'other_file.dll'), 'wb') \
                as other_file:
                other_file.write('MZ')
            subprocess.check_call([appx_exe(), '-o',
                                   os.path.join(d, 'test.appx'), d])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertIn('README.txt', zip.namelist())
                self.assertIn('somedir/other_file.dll', zip.namelist())
                self.assertNotIn('somedir', zip.namelist())
                self.assertNotIn('somedir/', zip.namelist())
                self.assertNotIn('test.appx', zip.namelist())

    def test_file_mapping(self):
        with appx.util.temp_dir() as d:
            with open(os.path.join(d, 'README.txt'), 'wb') as readme:
                readme.write('This is a test file.\n')
            with open(os.path.join(d, 'other_file.dll'), 'wb') as other_file:
                other_file.write('MZ')
            subprocess.check_call([
                appx_exe(), '-o', os.path.join(d, 'test.appx'),
                'README.txt={}'.format(os.path.join(d, 'README.txt')),
                'somedir/other_file.dll={}'.format(
                    os.path.join(d, 'other_file.dll')),
                ])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertIn('README.txt', zip.namelist())
                self.assertNotIn('other_file.dll', zip.namelist())
                self.assertIn('somedir/other_file.dll', zip.namelist())

if __name__ == '__main__':
    unittest.main()
