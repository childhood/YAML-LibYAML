use t::TestYAMLTests tests => 15;
use Devel::Peek();

my $rx1 = qr/5050/;
my $yaml1 = Dump $rx1;

is $yaml1, <<'...', 'Regular regexp dumps';
--- !!perl/regexp (?-xism:5050)
...

my $rx2 = qr/99999/;
bless $rx2, 'Classy';
my $yaml2 = Dump $rx2;

is $yaml2, <<'...', 'Blessed regular regexp dumps';
--- !!perl/regexp:Classy (?-xism:99999)
...

my $rx3 = qr/^edcba/mi;
my $yaml3 = Dump $rx3;

is $yaml3, <<'...', 'Regexp with flags dumps';
--- !!perl/regexp (?mi-xs:^edcba)
...

my $rx4 = bless $rx3, 'Bossy';
my $yaml4 = Dump $rx4;

is $yaml4, <<'...', 'Blessed regexp with flags dumps';
--- !!perl/regexp:Bossy (?mi-xs:^edcba)
...

my $rx1_ = Load($yaml1);
is ref($rx1_), 'Regexp', 'Can Load a regular regexp';
is $rx1_, '(?-xism:5050)', 'Loaded regexp value is correct';
like "404050506060", $rx1_, 'Loaded regexp works';

my $rx2_ = Load($yaml2);
is ref($rx2_), 'Classy', 'Can Load a blessed regexp';
is $rx2_, '(?-xism:99999)', 'Loaded blessed regexp value is correct';
ok "999999999" =~ $rx2_, 'Loaded blessed regexp works';

my $rx3_ = Load($yaml3);
is ref($rx3_), 'Regexp', 'Can Load a regexp with flags';
is $rx3_, '(?mi-xs:^edcba)', 'Loaded regexp with flags value is correct';
like "foo\neDcBA\n", $rx3_, 'Loaded regexp with flags works';

my $rx4_ = Load("--- !!perl/regexp (?msix:123)\n");
is ref($rx4_), 'Regexp', 'Can Load a regexp with all flags';
is $rx4_, '(?msix:123)', 'Loaded regexp with all flags value is correct';
