use t::TestYAML tests => 5;

require YAML::LibYAML;

open IN, "Changes" or die $!;
my $yaml = do {local $/; <IN>};
my @changes = YAML::LibYAML::Load($yaml);

pass "Changes file Load-ed without errors";
ok @changes == 3,
    "There are 3 Changes entries";
is $changes[0]->{version}, $YAML::LibYAML::VERSION,
    "Changes file jives with current version";
is $changes[-1]->{date}, 'Fri May 11 14:08:54 PDT 2007',
    "Version 0.01 is from Fri May 11 14:08:54 PDT 2007";
is ref($changes[-3]->{changes}), 'ARRAY',
    "Version 0.03 has multiple changes listed";
