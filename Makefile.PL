use inc::Module::Install;

name            'YAML-LibYAML';
all_from        'lib/YAML/LibYAML.pm';

use_test_base;
build_subdirs    'LibYAML';

WriteAll;
