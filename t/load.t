use t::TestYAML tests => 5;

spec_file('t/data/basic.t');
filters {
    yaml => ['parse_to_byte'],
    perl => ['eval'],
};

run_is_deeply yaml => 'perl';

sub parse_to_byte {
    require YAML::LibYAML;
    YAML::LibYAML::Load($_);
}
