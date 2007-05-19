use t::TestYAML tests => 1;

use YAML::LibYAML;

sub libyaml {
   YAML::LibYAML::Dump( $_[0] );
}

my @x = (256, 'xxx', libyaml({foo => 'bar'}));

isnt "@x", '256 xxx 256 xxx 256',
    "YAML::LibYAML::XS doesn't mess up the call stack";
