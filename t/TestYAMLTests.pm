package t::TestYAMLTests;
use Test::Base -Base;
use YAML::Tiny;
@t::TestYAMLTests::EXPORT = qw(Load Dump);

my $yaml_module;
BEGIN {
    my $config = {};
    my $config_file = 't/yaml_tests.yaml';
    if (-f $config_file) {
        ($config) = YAML::Tiny::LoadFile($config_file);
        if ($config->{use_blib}) {
            eval "use blib; 1" or die $@;
        }
    }
    $yaml_module = $ENV{PERL_YAML_TESTS_MODULE} || $config->{yaml_module}
      or die;
    eval "require $yaml_module; 1" or die $@;
}

sub Load() {
    no strict 'refs';
    &{$yaml_module . "::Load"}(@_);
}
sub Dump() {
    no strict 'refs';
    &{$yaml_module . "::Dump"}(@_);
}

no_diff;
delimiters ('===', '+++');
