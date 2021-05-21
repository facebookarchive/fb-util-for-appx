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

class TestEmptyFile(unittest.TestCase):
    '''
    Ensures the appx tool compresses empty files correctly.
    '''

    def test_empty_file(self):
        with appx.util.temp_dir() as d:
            filename = 'hello.world'
            file_path = os.path.join(d, filename)
            with open(file_path, 'w') as _:
                pass
            subprocess.check_call([appx_exe(),
                                   '-o', os.path.join(d, 'test.appx'),
                                   '-c', appx.util.test_key_path(),
                                   '-9', file_path])
            with zipfile.ZipFile(os.path.join(d, 'test.appx')) as zip:
                self.assertEqual(zip.getinfo(filename).compress_size, 2)

if __name__ == '__main__':
    unittest.main()
