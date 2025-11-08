# lit configuration for xunused tests
import os
from lit.formats import ShTest

config.name = 'xunused lit-tests'
config.test_format = ShTest()
config.suffixes = ['.c', '.cpp', '.test', '.sh']
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.test_source_root, 'exec')

# Tests assume 'xunused' is on PATH (CI builds it and adds to PATH)
