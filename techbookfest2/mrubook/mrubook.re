= はじめに

本書は主に Rubyist をターゲットに、軽量 Ruby 実装である mruby の言語処理系の実装を解説していきます。

想定する読者としては Ruby でコードを書いたことがある人、 Ruby の言語処理系に興味がある人としています。
そのため本書では Ruby の文法についていちから説明するなどといった記述はしません。あらかじめご了承ください。

筆者の目指す本書のゴールとしては、 mruby の特徴やその実装に関する造詣を深めていただき、 mruby に対する興味を深めていただいたり、もっと欲を言うと mruby コミュニティに参加していただくきっかけになることとしています。
本書が、あなたにとって mruby という新しい世界に踏み込むきっかけになれたら嬉しいです。

== mruby とは

さっそく mruby の実装の話に突入したいところですが、流石に急すぎると思われるので一旦簡単に mruby 自体について解説いたします。
あなたが「mruby は既にある程度知ってるよ！」という方でしたら、次の章までスキップしていただければ幸いです。

mruby は先述の通り軽量、省メモリ Ruby の実装です。
また機能もある程度 Ruby から削減されています。具体的には Ruby の JIS/ISO 規格に記載がある機能は標準で搭載されているのですが、それ以外はオプションな立ち位置で組み込むが ON/OFF が可能だったり、そもそも実装されていなかったりします。

主な用途としては組み込み環境向けでの利用が想定されます。
また他のアプリケーションに組み込んで使用するパターン（例えば、 matsumoto-r さんが開発した mod_mruby や ngx_mruby など）や、コマンドラインツールを実装するための言語として使用（mruby-cli）されていたりもします。

さらに mruby のサードパーティライブラリは mrbgem という単位で管理・配布可能です。ちょうど Ruby の RubyGems に相当します。
mrbgem は mruby をビルドする際の設定ファイルにそれを利用する記述することで、ビルドされたバイナリにリンクされる形で取り込まれます。

= 言語処理系コア

では mruby の実装の話に入っていきましょう。まず触れるのは、 mruby を mruby たらんとさせている言語処理系の話から初めます。

== 中間表現

mruby の VM が解釈する中間表現のコードは mruby としては iseq(instruction sequence) と呼ばれます。
これはソースコード上は mrb_code という uint32_t 型の別名として表現されています。

また iseq にシンボル情報などを付与したものは irep (internal representation) という名前で呼ばれています。

言語処理系のコンパイラや VM の実装を読む前に、まずはこの irep の中身を探索してみましょう。

=== irep (internal representation)

irep は include/mruby/irep.h で定義される、 mruby のコードの実行に必要な情報を保持する基本的なデータ構造です。
大きく分けて、実行するコード自体のデータ iseq 、ローカル変数やプールなどの情報、ソースコードのファイル名などのデバック情報を含みます。
詳細は下記に示すとおりです。

//table[irep][irep の構造]{
名前	内容 
-------------------------------------------------------------
nlocals	ローカル変数の個数
nregs	レジスタの個数
flags	irep の解釈の仕方を制御するためのフラグ
iseq	コードセグメントの先頭番地へのポインタ。実体としては 32 ビット整数値(mrb_code) のポインタ
pool	文字列、浮動小数点数、整数リテラルから生成したオブジェクトを格納する領域
syms	変数名やメソッド名などの解決のためのシンボルテーブル
reps	参照しうる他の irep へのポインタのリスト
lv	ローカル変数配列。実体としてはシンボルと符号なし整数値のペアの配列
ilen	iseq の長さ
plen	pool の長さ
slen	syms の長さ
rlen	reps の長さ
refcnt	TODO
//}

=== RProc

mruby の Proc オブジェクト型です。
mruby ではコンパイラがこのオブジェクトを生成し、 VM が実行することで mruby で処理を行う形になります。
RProc のデータ構造は下記の通りになります。

//table[RProc][RProc の構造]{
名前	内容 
-------------------------------------------------------------
MRB_OBJECT_HEADER	オブジェクトのヘッダ
body	具体的な処理の本体。実体は irep と関数ポインタの union
target_class	TODO
env	TODO
//}

== mruby コンパイラ

では実際に mruby のソースコードを読みながら処理を追ってみます。
mruby における字句解析、構文解析、 iseq 生成部分の実装は mrbgems/mruby-compiler/ に存在します。
mruby のコンパイラの仕事は mruby のスクリプトを解釈して、 RiteVM が実行可能な iseq を出力することにあります。

iseq を得るまでの全体の流れは下記の通りになります。
* なんか全体の流れがわかるいい感じの図

mruby の字句解析・構文解析の実装は parse.y にあります。
具体的にはスキャナ（字句解析器）が parse.y の parser_yylex() 、パーサ（構文解析器）が yyparse() が該当します。
・・・そしてこの辺りの基本的な設計は CRuby と同じだったりします。
「Rubyソースコード完全解説」@<bib>{rhg} という素晴らしい書籍に詳細が掲載されていますので、 CRuby と共通している部分の解説は省かせていただこうと思います。
詳しく知りたい方はそちらも合わせてご参照ください。

parser_yylex() は nextc() でソースコードを少しずつ読み出し、 switch 文でトークンを識別し、トークンの内容を返します。
またキーワードの検出については lex.def で定義される mrb_reserved_word() を呼んでいます。
これは、keywords に定義されている mruby のキーワードを検出するために GNU Perfect(gperf) で完全ハッシュ関数を生成する形で実装されています。
yyparse() は yacc の BNF によるシンタックスの定義から自動生成されます。

コード生成を行っているのは codegen.c となります。
このソースコードにおいて読解のエントリポイントとなるのは mrb_generate_code() です。
この関数は mrb_state 変数と parser_state 変数を取ってコード生成を行い、その結果を RProc の形式で返却します。
mrb_generate_code() を正常系に絞って追っていきますと、大筋としては codegen() で irep の生成を行い、それを用いて mrb_proc_new() で RProc オブジェクトを生成しているのが分かります。

mrb_generate_code() は引数に渡された、字句・構文解析の状態を保存するためのデータ構造 parser_state から AST のルートノードへのポインタを取り出して codegen() に渡します。
構文木はリストの構造になっており、先頭要素を取り出すメンバ car と続くリストを取り出すメンバ cdr を持ちます。
codegen() では基本的に car で取り出したノードのタイプによって switch 文で分岐し、コード生成を行い、 iseq の末尾に追加していきます。

ノードのタイプは 103 種類ほどありコード生成について逐一詳細を追っていくのは困難です。
ここでは取っ掛かりとして、下記のような典型的な "Hello,World!" と標準出力するだけの mruby スクリプトの irep を確認し、その構成要素について追ってみます。

//listnum[hello.rb][mruby スクリプト例][ruby]{
puts "Hello,World!"
//}

mruby では --verbose オプションを付けることで構文解析の結果と irep を確認することができます。
早速上記のスクリプトを用いて確認してみます。

//cmd{
 $ ./bin/mruby --verbose hello.rb
00001 NODE_SCOPE:
00001   NODE_BEGIN:
00001     NODE_CALL:
00001       NODE_SELF
00001       method='puts' (383)
00001       args:
00001         NODE_STR "Hello,World!" len 12
...

Hello,World!
//}

このサンプルでは NODE_SCOPE 、 NODE_BEGIN 、 NODE_CALL 、 NODE_SELF 、 NODE_STR というノードが得られたようです。
それぞれ codegen() でどのように処理されるのか見ていきます。

まず先頭ノードの、スクリプトの一番外側を示す NODE_SCOPE ですが、 scope_body() を呼んでいるだけです。
scope_body() ではまず codegen() を呼び出し次のノードのコード生成を行います。
それが終わると STOP 命令もしくは RETURN 命令を追加します。
ちなみにコード生成は MKOP_A などのマクロによって生成されます。コード生成マクロは取りうるオペランドによってバリエーションがあります。
そして生成されたコードの iseq への追加は genop() で行われます。

次は NODE_BEGIN です。
NODE_BEGIN ではツリーが辿れない場合は NODENIL 命令を追加、辿れる場合は辿れる分まで codegen() を while ループで呼び出してコード生成を行います。

NODE_CALL の処理は gen_call() にまとめられています。
gen_call() ではまずリストの次の要素を codegen() に渡します。
これがメソッドのレシーバとなる想定であり、上記コードなら NODE_SELF がそれに該当します。
〜〜TODO〜〜
最後に、呼び出すのがブロックでなければ SEND 命令、ブロックであれば SENDB 命令を追加します。

NODE_SELF はシンプルで、 genop() で LOADSELF 命令を追加するだけです。

最後に NODE_STR ですが、まずリストの後続 2 要素を取り出します。
これらはそれぞれ char* 型の文字列先頭へのポインタと文字列のサイズを意味する値が入っています。
その後 mrb_str_new() を使って、この 2 変数から String 型オブジェクトを生成し、 new_lit() に渡します。
new_lit() は渡されたオブジェクトを irep の pool 領域に格納します。
ちなみにこの new_lit() は文字列の他にも整数、浮動小数点数リテラルから生成したオブジェクトを保存するのにも使用します。

== Rite VM

mruby の一番の特徴は、 mruby の VM (RiteVM) にあるとも考えられるでしょう。
RiteVM についてつらつらと書いていきます。

=== アーキテクチャ

RiteVM の実装を読み解く上で命令セットアーキテクチャの理解は不可欠です。
というわけで、まずは mruby の命令について理解を深めてみましょう。
なお、ソースコード上は include/mruby/opcode.h がこの内容に該当します。

RiteVM はレジスタマシンとして動作します。
1 つの命令は 32 ビットで表現され、 7 ビットのオペコードと 0 個以上 3 個以上のオペランドを取る形式になり、下記のようなバリエーションが存在します。

//table[regs][レジスタ種別]{
名前	内容 
-------------------------------------------------------------
A	9 ビット長のレジスタ
Ax	25 ビット長のレジスタ
B	9 ビット長のレジスタ
Bx	16 ビット長のレジスタ
sBx	16 ビット長のレジスタ(符号付き)
C	7 ビット長のレジスタ
//}

mruby の命令は下記のようになります。実装の詳細については後述の RiteVM の実装のときに触れます。

//table[opcode_loads][基本ロード命令]{
命令	内容
-------------------------------------------------------------
MOVE A B	A が指すレジスタに B が指すレジスタの値をコピーする
LOADL A Bx	A が指すレジスタに Bx の値番目の pool の値をコピーする
LOADI A sBx	A が指すレジスタに sBx の値をコピーする
LOADSYM A Bx	A が指すレジスタに Bx の値番目の syms のシンボルをコピーする
LOADSELF A	A が指すレジスタに self の値をコピーする
LOADSELF A	A が指すレジスタに self の値をコピーする
LOADTRUE A	A が指すレジスタに true をコピーする
LOADFALSE A	A が指すレジスタに false をコピーする
LOADNIL A	A が指すレジスタに nil をコピーする
//}

//table[opcode_variables][変数操作命令]{
命令	内容
-------------------------------------------------------------
GETGLOBAL A Bx	A が指すレジスタに Bx が指すレジスタ番目の syms のシンボルが指すグローバル変数の値をコピーする
SETGLOBAL A Bx	A が指すレジスタの値を Bx が指すレジスタ番目の syms のシンボルが指すグローバル変数の値としてセットする
GETIV A Bx	A が指すレジスタに Bx が指すレジスタ番目の syms のシンボルが指す self オブジェクトのインスタンス変数の値をコピーする
SETIV A Bx	A が指すレジスタの値を Bx が指すレジスタ番目の syms のシンボルが指す self オブジェクトのインスタンス変数の値としてセットする
GETCV A Bx	A が指すレジスタに Bx が指すレジスタ番目の syms のシンボルが指すクラス変数の値をコピーする
SETCV A Bx	A が指すレジスタの値を Bx が指すレジスタ番目の syms のシンボルが指すクラス変数の値としてセットする
GETCONST A Bx	A が指すレジスタに Bx が指すレジスタ番目の syms のシンボルが指す定数の値をコピーする
SETCONST A Bx	A が指すレジスタの値を Bx が指すレジスタ番目の syms のシンボルが指す定数の値としてセットする
GETMCNST A Bx	A が指すレジスタに、 A が指すレジスタが指すモジュールのうち Bx が指すレジスタ番目の syms のシンボルが指す定数の値をコピーする
SETMCNST A Bx	A が指すレジスタの値を、 A が指すレジスタが指すモジュールのうち Bx が指すレジスタ番目の syms のシンボルが指す定数の値としてセットする
GETUPVAR A B C	TODO
SETUPVAR A B C	TODO
//}

//table[opcode_controlls][制御命令]{
命令	内容
-------------------------------------------------------------
JMP sBx	sBx 分だけプログラムカウンタを増やす
JMPIF A sBx	A が指すレジスタの値の評価結果が true だった場合、 sBx 分だけプログラムカウンタを増やす
JMPNOT A sBx	A が指すレジスタの値の評価結果が false だった場合、 sBx 分だけプログラムカウンタを増やす
ONERR sBx	プログラムカウンタに sBx 加算した値を rescue 時処理として実行コンテキストに追加する
RESCUE A	A が指すレジスタに mrb_state の exc をコピーし、 exc をクリアする
POPERR A	実行コンテキストから A の値分だけ rescue 時処理を削除する
RAISE A	A が指すレジスタを proc オブジェクトとして解釈して mrb_state の exc にコピーして例外処理をする
EPUSH Bx	プログラムカウンタに sBx 加算した値を ensure 時処理として実行コンテキストに追加する
POPERR A	実行コンテキストから A の値分だけ ensure 時処理を呼び出しつつ削除する
SEND A B C	A が指すレジスタのオブジェクトをレシーバとし、 B が指すシンボルのメソッドを C が指す引数の個数で呼び出す
CALL A	self を Proc オブジェクトとみなして呼び出す
SUPER A C	レシーバの親クラスのメソッドを C が指す引数の個数で呼び出す
ARGARY A Bx	TODO
ENTER Ax	TODO
RETURN A B	TODO
TAILCALL A B C	TODO
BLKPUSH A Bx	TODO
//}

二項演算命令ではレジスタの値の型が数値、浮動小数点数、文字列かどうかにより結果が変わります。
これらの条件にマッチしない場合は代わりに SEND 命令を実行します。

//table[opcode_operations][二項演算命令]{
命令	内容
-------------------------------------------------------------
ADD A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に加算結果、文字列同士の場合に結合結果をセットする
SUB A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に減算結果をセットする
MUL A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に乗算結果をセットする
DIV A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に除算結果をセットする
ADDI A B C	A が指すレジスタに、 A が指すレジスタと C の値の加算結果をセットする
SUBI A B C	A が指すレジスタに、 A が指すレジスタと C の値の減算結果をセットする
EQ A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に比較結果をセットする
LT A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に後者の方が大きい値かどうかをセットする
LE A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に後者の方が大きい値もしくは同値どうかをセットする
GT A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に後者の方が小さい値かどうかをセットする
GE A B C	A が指すレジスタに、 A が指すレジスタと A+1 が指すレジスタが整数と浮動小数点数の組み合わせの際に後者の方が小さい値もしくは同値どうかをセットする
//}

//table[opcode_array][配列操作命令]{
命令	内容
-------------------------------------------------------------
ARRAY A B C	A が指すレジスタに、 B から B + C 番目までのレジスタに格納されている値をコピーして Array オブジェクトをセットする
ARYCAT A B	A が指すレジスタに、 A が指すレジスタと B が指すレジスタの Array オブジェクトを結合してセットする
ARYPUSH A B	A が指すレジスタにセットされている Array オブジェクトに、 B が指すレジスタの値を末尾に追加する
AREF A B C	A が指すレジスタに、 B が指すレジスタにセットされている Array オブジェクトの C 番目の要素をセットする
ASET A B C	A が指すレジスタの値を B が指すレジスタにセットされている Array オブジェクトの C 番目の要素としてセットする
APOST A B C	TODO
//}

//table[opcode_objects][オブジェクト操作命令]{
命令	内容
-------------------------------------------------------------
STRING A Bx	A が指すレジスタに Bx 番目の pool に格納された String オブジェクトを複製した結果をセットする
STRCAT A B	A が指すレジスタに、 A と B が指すレジスタに格納された String オブジェクトを結合した結果をセットする
HASH A B C	A が指すレジスタに、 B から B + C * 2 番目のレジスタに格納されているオブジェクトを、キーとバリューのペアの繰り返しとみなして読み出し Hash オブジェクトを生成して、その結果をセットする
LAMBDA A B C	TODO
OCLASS A	TODO
CLASS A	TODO
MODULE A B	TODO
EXEC A Bx	TODO
METHOD A B	TODO
SCLASS A B	TODO
TCLASS A	TODO
RANGE A B C	TODO
//}

//table[opcode_etc][その他の命令]{
命令	内容
-------------------------------------------------------------
NOP	なにもしません
DEBUG A B C	mruby のビルド時のオプションによって動作が異なります。 mrb_state の debug_op_hook() 呼び出しかデバッグ情報の printf 出力、もしくは abort() します
STOP	現在の実行コンテキストの ensure ブロックの呼び出しを行った後、ジャンプ先の設定をし、例外オブジェクトもしくは irep->nlocals 番目のレジスタの値を return します
ERR A Bx	pool の Bx 番目の要素を例外メッセージとしてとり、 A が 0 なら RuntimeError、それ以外なら LocalJumpError を raise します
//}

=== VM の状態を示すデータ構造 mrb_state 

mrb_state は VM の実行状態情報を保持するデータ構造です。下記のような情報を保持します。

//table[mrb_state][mrb_state の構造]{
名前	内容 
-------------------------------------------------------------
c, root_c	実行コンテキスト
exc	例外ハンドラへのポインタ
globals	グローバル変数テーブル
*_class	Object や Class, String など基本クラスへのポインタ
gc	GC 情報 TODO
//}

=== 実行コンテキスト mrb_context

mrb_context は VM の実行コンテキストを保持するデータ構造です。
この構造は Fiber を実現するために使われています。
始点となるルート実行コンテキスト（mrb_state の root_c）から、 fiber を生成するたびにそれに対応する実行コンテキストが生成されるイメージです。

mrb_context では下記のような情報を保持します。

//table[mrb_context][mrb_context の構造]{
名前	内容 
-------------------------------------------------------------
prev	自身を呼び出した側の実行コンテキスト。 root_c か親 fiber になる
stack, stbase, stend	RiteVM のレジスタ情報（ただし 0 番目の要素は self オブジェクトを指す用途で使用）
ci, cibase, cind	メソッド呼び出し情報
rescue, rsize	rescue ハンドラに関する情報
ensure, esize	ensure ハンドラに関する情報
status, fib	そのコンテキストの持ち主の fiber の実行状態とその fiber へのポインタ
//}

せっかく Fiber の話題が出たので、 Fiber の中身についても少しだけ触れてみます。
Fiber は実行コンテキストを保持するオブジェクトです。
ソースコード中では RFiber という構造体で表現されています。

//table[RFiber][RFiber の構造]{
名前	内容 
-------------------------------------------------------------
MRB_OBJECT_HEADER	オブジェクトのヘッダ
cxt	その Fiber オブジェクトの実行コンテキスト
//}

ちなみに mruby では Fiber は mrbgems として切り出されています。
つまりは不要なら Fiber を含まない形でビルドできる訳ですね・・・！

=== メソッド呼び出し情報 mrb_callinfo

mrb_callinfo は mruby のメソッド呼び出しに関わるデータ構造です。

mrb_callinfo 下記のような情報を保持します。

//table[mrb_callinfo][mrb_callinfo の構造]{
名前	内容 
-------------------------------------------------------------
proc	呼び出されたメソッドの Proc オブジェクト
nregs	レジスタ数
ridx, eidx	rescue, ensure 呼び出しのネストの深さ
env	？？？
//}


=== VM のエントリポイント

それでは実際に VM の実装を見ていきましょう。
まずは include/mruby.h で宣言されている、ユーザが目にする関数を足がかりに処理を追っていきます。

これらのエントリポイントとなる関数としては下記が挙げられます。

 * mrb_run()
 * mrb_toplevel_run_keep()
 * mrb_toplevel_run()

この 3 つの関数は、 mrb_state と RProc をとり VM の実行を開始します。
複雑な処理はしておらず、メインの処理は mrb_context_run() に任せているように見えます。
というわけで次は mrb_context_run() を覗いてみましょう。

=== VM のメイン処理

前述の mrb_context_run() は mruby の VM のメイン処理です。
この関数は行数が多くギョッとするかも知れませんが、行っていることはかなりシンプルです。
大部分の処理のフローとしては 1) INIT_DISPATCH マクロでオペコードをフェッチ 2) switch 文でオペコードを判別 3) 命令の実行 を for 文でループする形になります。

==== 初期化処理

まず命令処理のループに入る前に初期化処理を行います。
ここで mrb_context_run() の引数の mrb_state の値の c メンバから stack メンバを取り（mrb->c->stack）、 0 番目の要素を self オブジェクト、それ以降をレジスタの保存先とします。
また c メンバの ci メンバ（mrb->c->ci）の proc メンバと ngres メンバをそれぞれ設定します。

==== RiteVM 命令処理

次に一部命令の実装について触れていってみます。
ロード系命令ではレジスタ間の値のコピーを行ったり、レジスタに任意の値をセットしたりします。
これらの実装としてはシンプルです。
例えば MOVE 命令は mrb_context の stack メンバから得たレジスタを対象にレジスタ間の値のコピーを行います。

変数操作系命令では基本的に 2 つのオペランドを取ります。
GET 操作では取得対象の変数名をレジスタから取得してシンボルを得て、値を GET するための関数を呼び出し、その結果をもう一方のレジスタに格納します。
SET 操作では値をレジスタから値を、 SET 先のシンボルをもう一方のレジスタから得て SET するための関数を呼び出します。
GET/SET するための関数は操作対象の変数の性質によって異なり、例えばグローバル変数であれば mrb_gv_get() / mrb_gv_set() を呼び出します。

制御系命令ではジャンプをしたり例外処理をしたり、メソッド呼び出しをします。
JMP, JMPIF, JMPNOT 命令はオペランドの条件によってプログラムカウンタを増加させて次ループに移るだけです。
例外処理のための命令はやや複雑です。簡単に分類すると RAISE は raise に関する命令、 ONERR, POPERR, RESCUE は rescure に関する命令、 EPUSH, EPOP は ensure に関する命令になります。
基本的に前のページで述べた通りなのですが、これらの命令は実行コンテキスト（mrb->c） やそのメソッド呼び出し情報（mrb->c->ci）の例外に関する情報を更新します。
ちなみに rescue, ensure ブロックを保存する配列のサイズが足りなくなった際、最初は 16 要素分、その後不足が出た際に 32, 64 と倍々に mrb_realloc() で領域を確保し直します。

==== RiteVM による実際の irep の解釈例

ある程度 RiteVM の実装の形が見えてきたところで、実際の irep を確認してみましょう。
コード生成部分で触れた、単純な Hello,World! スクリプトから生成される irep は下記の通りになります。

//cmd{
irep 0x7fdba9c0b9b0 nregs=4 nlocals=1 pools=1 syms=1 reps=0
file: hello.rb
    1 000 OP_LOADSELF   R1
    1 001 OP_STRING     R2      L(0)    ; "Hello,World!"
    1 002 OP_SEND       R1      :puts   1
    1 003 OP_STOP
//}

生成された irep は 1 個であり、他の irep を参照しません。（reps=0）
また、 pools の要素は 1 （"Hello,World!" を指す String オブジェクトでしょう）、 syms の数は 1 （puts メソッドの名前でしょう）のようです。

= Ruby との比較

== khash

== ガベージコレクション

= おわりに

