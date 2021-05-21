#
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import contextlib
import os
import shutil
import tempfile

@contextlib.contextmanager
def temp_dir():
    dir = tempfile.mkdtemp()
    try:
        yield dir
    finally:
        shutil.rmtree(dir)

def appx_exe():
    path = os.getenv('FB_APPX_PATH')
    if path is None:
        raise Exception('FB_APPX_PATH environment variable must be specified')
    return path

def test_dir_path():
    return os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

def test_key_path():
    return os.path.join(test_dir_path(), 'App_TemporaryKey.pfx')
