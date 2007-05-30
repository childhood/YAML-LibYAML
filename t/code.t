use t::TestYAMLTests tests => 2;

my $sub = sub { print "Hi.\n" };

my $yaml = <<'...';
--- !!perl/code: '{ "DUMMY" }'
...

is Dump($sub), $yaml,
    "Dumping a code ref works";

bless $sub, "Barry::White";

$yaml = <<'...';
--- !!perl/code:Barry::White '{ "DUMMY" }'
...

is Dump($sub), $yaml,
    "Dumping a blessed code ref works";

