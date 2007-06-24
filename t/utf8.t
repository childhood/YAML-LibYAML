use t::TestYAMLTests tests => 4;

my $hash = {
    '店名' => 'OpCafé',
    '電話' => <<'...',
03-5277806
0991-100087
...
    Email => 'boss@opcafe.net',
    '時間' => '11:01~23:59',
    '地址' => '新竹市 300 石坊街 37-8 號',
};

my $yaml = <<'...';
---
Email: boss@opcafe.net
地址: 新竹市 300 石坊街 37-8 號
店名: OpCafé
時間: 11:01~23:59
電話: '03-5277806

  0991-100087

'
...

is Dump($hash), $yaml, 'Dumping Chinese hash works';
is_deeply Load($yaml), $hash, 'Loading Chinese hash works';

my $hash2 = {
    'モジュール' => [
        {
            '名前' => 'YAML',
            '作者' => {'名前' => 'インギー', '場所' => 'シアトル'},
        },
        {
            '名前' => 'Plagger',
            '作者' => {'名前' => '宮川達彦', '場所' => 'サンフランシスコ' },
        },
    ]
};

my $yaml2 = <<'...';
---
モジュール:
- 作者:
    名前: インギー
    場所: シアトル
  名前: YAML
- 作者:
    名前: 宮川達彦
    場所: サンフランシスコ
  名前: Plagger
...

is Dump($hash2), $yaml2, 'Dumping Japanese hash works';
is_deeply Load($yaml2), $hash2, 'Loading Japanese hash works';
