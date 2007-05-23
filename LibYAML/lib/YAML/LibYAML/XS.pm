package YAML::LibYAML::XS;
use 5.008003;
use strict;
use warnings;

use XSLoader;
XSLoader::load 'YAML::LibYAML::XS';
use base 'Exporter';

our @EXPORT_OK = qw(Load Dump);

sub Dump {
    return '' unless @_;
    return DumpXS(@_);
}

1;

=head1 NAME

YAML::LibYAML::XS - An XS Wrapper Module of libyaml

=cut
