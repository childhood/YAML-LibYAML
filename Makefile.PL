use inc::Module::Install;

name            'YAML-LibYAML';
all_from        'lib/YAML/LibYAML.pm';

use_test_base;
use_yaml_tests;

build_subdirs    'LibYAML';

WriteAll;
