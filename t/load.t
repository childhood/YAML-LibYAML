use t::TestYAML tests => 4;

spec_file('t/data/1.t');
filters {
    yaml => ['parse_to_byte'],
    perl => ['eval'],
};

run_is_deeply yaml => 'perl';

sub parse_to_byte {
    require YAML::LibYAML;
    YAML::LibYAML::Load($_);
}
