use t::TestYAML tests => 4;

spec_file('t/data/1.t');
filters {
    perl => ['eval', 'test_dump'],
};

run_is perl => 'libyaml_emit';

sub test_dump {
    require YAML::LibYAML;
    YAML::LibYAML::Dump(@_) || "Dump failed";
}

