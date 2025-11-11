# lit configuration for xunused tests
import os
import sys
import lit.formats
from lit.llvm import llvm_config

config.name = 'xunused lit-tests'
config.test_format = lit.formats.ShTest()
config.suffixes = ['.c', '.cpp', '.test', '.sh']

# Get the source and build directories from the site config if available
if hasattr(config, 'xunused_src_root'):
    config.test_source_root = os.path.join(config.xunused_src_root, 'tests')
else:
    config.test_source_root = os.path.dirname(__file__)

if hasattr(config, 'xunused_obj_root'):
    config.test_exec_root = os.path.join(config.xunused_obj_root, 'tests', 'exec')
else:
    config.test_exec_root = os.path.join(config.test_source_root, 'exec')

# Use llvm_config to add tool substitutions (shows full paths in test output)
if hasattr(config, 'xunused_tools_dir') and hasattr(config, 'llvm_tools_dir'):
    tools = ['xunused', 'FileCheck', 'split-file']
    tool_dirs = [config.xunused_tools_dir, config.llvm_tools_dir]
    llvm_config.add_tool_substitutions(tools, tool_dirs)
