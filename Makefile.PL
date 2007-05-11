use inc::Module::Install;

name            'YAML-LibYAML';
all_from        'lib/YAML/LibYAML.pm';

requires        'Class::Field' => 0;

use_test_base;
build_subdirs    'LibYAML';

WriteAll;
