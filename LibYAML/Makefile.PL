use ExtUtils::MakeMaker;
use strict;
use Config;

my $obj_files = join ' ', map {
    my $c = $_;
    $c =~ s/\.c$/$Config::Config{_o}/;
    $c;
} glob("*.c"), 'XS.c';

WriteMakefile(
    NAME => 'YAML::LibYAML::XS',
    PREREQ_PM => {},
    DEFINE => '-DHAVE_CONFIG_H',
    LIBS => [''], # e.g., '-lm'
    INC => '-I. -Iinclude',
    OBJECT => $obj_files,
    ABSTRACT_FROM => 'lib/YAML/LibYAML/XS.pm',
    AUTHOR => 'Ingy döt Net <ingy@cpan.org>',
);
