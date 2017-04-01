= 手書きで irep

このオマケの章では筆者の個人的な興味により、コンパイラを頼らず irep を手で書いて RiteVM に実行させることを試してみようと思います。


== mrbc コマンドと mruby コマンド

（明示的にビルドしないような設定を記述しない限り） mruby をビルドすると、 ./bin/ に mrbc と mruby というバイナリが出力されます。
mrbc は mruby スクリプトをコンパイルして、 .mrb 拡張子の irep をダンプした形式（以下、 Rite バイナリ）のファイルを出力します。

//cmd{
# hello.mrb というファイルが出力される
$ ./bin/mrbc hello.rb
//}

mruby は mruby スクリプトをコンパイルした後 その Rite バイナリを実行します。
-b オプションで mrbc に出力させたファイルを実行することもできます。

//cmd{
# hello.rb を直接実行
$ ./bin/mruby hello.rb
Hello,World!

# mrbc で出力した hello.mrb を実行
$ ./bin/mruby -b hello.mrb
Hello,World!
//}


== mrbc が出力するファイルの形式

mrbc が出力する Rite バイナリは @<img>{rite_binary_format} のような形式を取ります。

//image[rite_binary_format][Rite バイナリの形式][scale=0.7]

Rite バイナリファイルのヘッダは include/mruby/dump.h にて定義される rite_binary_header 構造体で表現され、そのサイズは 22 バイトになります。
rite_binary_header 構造体は @<table>{rite_binary_header} のような情報を持ちます。

//tsize[30,90]
//table[rite_binary_header][rite_binary_header の構造]{
名前	内容 
-------------------------------------------------------------
binary_ident	RiteVM 用バイナリであることを示す識別子。内容は "RITE"
binary_version	バイナリバージョン。 mruby-1.2.0 では "0003"
binary_crc	バイナリ検証用の CRC
binary_size	バイナリのサイズ
compiler_name	コンパイラ名。 mruby-1.2.0 では "MATZ"
compiler_version	コンパイラバージョン。 mruby-1.2.0 では "0000"
//}

Rite バイナリのヘッダ以降のデータはセクションという単位で分割されます。
セクションにはいくつかの種類があるのですが、ここでは最低限抑えておくべき irep セクションについてのみ触れます。

irep セクションは irep セクションヘッダと irep レコードからなります。
irep セクションヘッダを表現する構造体では @<table>{rite_section_irep_header} のような情報を持ちます。

//tsize[30,80]
//table[rite_section_irep_header][rite_section_irep_header の構造]{
名前	内容 
-------------------------------------------------------------
section_ident	セクションの識別子。 irep セクションなら "IREP"
section_size	セクションのサイズ
rite_version	RiteVM のバージョン。 mruby-1.2.0 では "0000"
//}

irep セクションヘッダの後には irep レコードが続きます。
irep レコードはは下記のような情報を持ちます。

 * レコードサイズ（4バイト）
 * ローカル変数の数（2 バイト）
 * レジスタ数（2  バイト）
 * 参照する irep 数（2 バイト）
 * iseq 命令数（4 バイト）
 * iseq 命令列（4 バイト * iseq 命令数）
 * pool の要素数（4 バイト）
 * pool の要素列（サイズは内容による）
 * syms の要素数（4 バイト）
 * syms の要素列（サイズは内容による）
 * 参照する他の irep レコード（サイズは内容による）

Rite バイナリの末尾にはフッタが付与されます。
フッタの内容は下記の 8 バイトの情報からなります。

 * フッタの識別子（4 バイト） mruby-1.2.0 では "END\0"
 * フッタのサイズ（4 バイト）


== さあ手書きしてみよう

今回は題材として @<list>{plus.rb} のような mruby スクリプトと同じ動作をする Rite バイナリを手書きしてみましょう。

//listnum[plus.rb][加算するだけのスクリプト][ruby]{
puts 1 + 2
//}

実行結果として下記のような出力が得られることを期待します。

//cmd{
$ ./bin/mruby plus.rb
3
//}

=== irep セクション

まずは irep レコードを書いてみましょう。

今回書く iseq はシンプルに 1 と 2 をそれぞれレジスタにロードして加算し、その結果を puts で出力するような処理にしてみます。
これを書き下すと以下のように、 OP_LOADI で R2 に 1 を、 R3 に 2 をコピーした後 OP_ADD で加算して、 OP_SEND で puts メソッドにその結果を出力するような内容になります。
（その他の命令については第 2 章で既に触れたとおりなので割愛します）

//cmd{
OP_LOADSELF   R1
OP_LOADI      R2      1
OP_LOADI      R3      2
OP_ADD        R2      :+      0
OP_SEND       R1      :puts   1
OP_STOP
//}

iseq には命令に対応する番号と、オペランドの値を記述することになります。
上記の場合は @<table>{iseqs} に示すような値になります。

//tsize[50,50]
//table[iseqs][必要な iseq に関わる情報]{
命令(番号) オペランドの内容	iseq 上の表現
-------------------------------------------------------------
OP_LOADSELF(0x06) A=1	0x00800006
OP_LOADI(0x03) A=2, Bx=1	0x01400003
OP_LOADI(0x03) A=3, Bx=2	0x01c00083
OP_ADD(0x2c) A=2, B=0, C=0	0x0100402c
OP_SEND(0x20) A=0, B=1, C=1	0x008000a0
OP_STOP(0x4a) 指定なし	0x0000004a
//}

今回のサンプルスクリプトでは pool の要素はなしになります。
また syms は "puts" の分と "+" の分の 2 要素が必要となります。

以上のことから irep レコードを示すバイナリは下記のようになります。

//cmd{
# レコードサイズ 〜 iseq 命令数
00 00 00 3d 00 01 00 05 00 00 00 00 00 06

# iseq
## OP_LOADSELF
00 80 00 06
## OP_LOADI
01 40 00 03
## OP_LOADI
01 c0 00 83
## OP_ADD
01 00 40 2c
## OP_SEND
00 80 00 a0
## OP_STOP
00 00 00 4a

# pool（要素が無いので、サイズが 0 であることを指す情報だけ）
00 00 00 00

# syms（要素 2 で、 "puts" と "+"）
00 00 00 02
## "puts"
00 04 70 75 74 73 00
## "+"
00 01 2b 00

# フッタ（"END\0" とサイズ）
45 4e 44 00 00 00 00 08
//}

irep セクションの内容が定まれば irep セクションヘッダも記述することができます。
ここでは下記のような内容になります。

//cmd{
# section_ident（"IREP"）
49 52 45 50
# section_size（0x45）
00 00 00 45
# rite_version（"0000"）
30 30 30 30
//}

=== Rite バイナリヘッダ

基本的には @<table>{rite_binary_header} の内容を踏襲しようと思います。
ただしここではちょっとした遊び要素として、 compiler_name を本書の名前 "mrubook" にちなんで "MRBK" に変更してみます。

また binary_crc は src/crc.c に定義される calc_crc_16_ccitt() の戻り値を指定することになります。
ここでは @<list>{mycrc.c} に示すように、 src/crc.c を再利用して binary_crc 用の値を生成します。

//listnum[mycrc.c][binary_crc 生成コード][c]{
#include <stdio.h>
...
uint16_t
calc_crc_16_ccitt(const uint8_t *src, size_t nbytes, uint16_t crc)
...

int main()
{
  // binary_crc 以降の値を用意
  uint8_t buf[] = {
    0x00, 0x00, 0x00, 0x63,
    ...
    0x00, 0x00, 0x00, 0x08
  };

  printf("%x\n", calc_crc_16_ccitt(buf, sizeof(buf), 0));
  return 0;
}
//}

今回の内容ですと binary_crc は 0xdf36 になります。

以上のことから Rite バイナリヘッダは下記のようになります。

//cmd{
# binary_ident（"RITE"）
52 49 54 45
# binary_version（"0000"）
30 30 30 33
# binary_crc
df 36
# binary_size（0x63）
00 00 00 63
# compiler_name（"MRBK"）
4d 52 42 4b
# compiler_version（"0000"）
30 30 30 30
//}


=== 実際に動かしてみる

以上の内容をすべて踏まえると、実行する Rite バイナリは以下のような内容になります。

//cmd{
$ hexdump plus.mrb
0000000 52 49 54 45 30 30 30 33 df 36 00 00 00 63 4d 52
0000010 42 4b 30 30 30 30 49 52 45 50 00 00 00 45 30 30
0000020 30 30 00 00 00 3d 00 01 00 05 00 00 00 00 00 06
0000030 00 80 00 06 01 40 00 03 01 c0 00 83 01 00 40 2c
0000040 00 80 00 a0 00 00 00 4a 00 00 00 00 00 00 00 02
0000050 00 04 70 75 74 73 00 00 01 2b 00 45 4e 44 00 00
0000060 00 00 08
0000063
//}

実行すると、期待通りの出力が得られることが確認できます。
どうやら無事に irep の手書きに成功したようですね。

//cmd{
$ ./bin/mruby -b plus.mrb
3
//}

