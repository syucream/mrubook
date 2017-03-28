= 言語処理系コア

ここから mruby の実装の内容に踏み込んでいきます。
さてまずはどこから着手しましょうか？

「まつもとゆきひろ 言語のしくみ」@<bib>{lang_book} において、 Matz は mruby の開発において実行効率の良い VM に主眼を置き、まず VM の実装から着手したと語っています。
そんな mruby を mruby たらんとさせている言語処理系の話から触れていきましょう。

ざっくりと言語処理系コア部分の概要を表すと @<img>{lang_core} のようになります。
mruby のスクリプトは中間表現にコンパイルされて実行されます。
この中間表現を解釈して実行するのが mruby の Virtual Machine(VM) である RiteVM です。
またこの中間表現をコンパイルするのが、字句・構文解析器とコード生成器からなるコンパイラ部分です。

//image[lang_core][mruby の言語処理系コア部分][scale=0.7]

== 中間表現

RiteVM が解釈する中間表現のコードは mruby では iseq(Instruction Sequence) と呼ばれます。
これはソースコード上は mrb_code という uint32_t 型の別名として定義されています。

また iseq にシンボル情報などを付与したものは irep (Internal Representation) という名前で呼ばれています。
さらに RiteVM でコード実行する際には irep を包む形の RProc という構造を渡します。
これらの構造の関係は @<img>{ireps} のような入れ子の関係になっています。

//image[ireps][mruby の中間表現][scale=0.7]

=== irep (Internal Representation)

irep は mruby のコードの実行に必要な情報を保持する基本的なデータ構造です。
この構造は mruby のコードのスコープに対応します。
スコープの実現のため、実行するコード自体のデータ iseq に加え、ローカル変数やレジスタ、プールなどの情報を含みます。

詳細は @<table>{irep} に示すとおりです。
ソースコードは include/mruby/irep.h に定義されています。

//tsize[30,80]
//table[irep][irep の構造]{
名前	内容 
-------------------------------------------------------------
nlocals	ローカル変数の個数
nregs	レジスタの個数
flags	irep の解釈の仕方を制御するためのフラグ
iseq	コードセグメントの先頭番地へのポインタ。実体は 32 ビット整数値(mrb_code) のポインタ
pool	文字列、浮動小数点数、整数リテラルから生成したオブジェクトを格納する領域
syms	変数名やメソッド名などの解決のためのシンボルテーブル
reps	他の irep へのポインタのリスト
lv	ローカル変数配列。実体はシンボルと符号なし整数値のペアの配列
//}

=== RProc

mruby の Proc オブジェクト型です。
mruby ではコンパイラがこのオブジェクトを生成し、 RiteVM が実行することで mruby で処理を行う形になります。
RProc のデータ構造は @<table>{RProc} の通りになります。

//tsize[40,80]
//table[RProc][RProc の構造]{
名前	内容 
-------------------------------------------------------------
MRB_OBJECT_HEADER	オブジェクトのヘッダ
body	具体的な処理の本体。実体は irep と関数ポインタの union
target_class	Proc のレシーバオブジェクトのクラス
env	レジスタやシンボル情報を保存する REnv 構造体データ
//}


== mruby コンパイラ

mruby のコンパイラの実装は mrbgems/mruby-compiler/ に存在します。
こいつの仕事は mruby のスクリプトを解釈して、 RiteVM が実行可能な irep を出力することにあります。
ちなみに、 mrbgems/ 以下にあることからもわかるとおり、 mruby のコンパイラは mrbgem 扱いでありビルド時に含まないようにすることもできます。
使用可能なリソースが限られる組み込み機器に配慮した設計になっていると言えそうです。

mruby のコンパイラは大きく分けて字句・構文解析と コード生成部分があります。
これはちょうど @<img>{lang_core} のうち RiteVM の部分以外に該当します。
個別に深掘りしてみましょう。

=== 字句・構文解析

mruby の字句・構文解析の実装は parse.y にあります。
具体的にはスキャナ（字句解析器）が parse.y の parser_yylex() 、パーサ（構文解析器）が yyparse() が該当します。
この辺りの基本的な設計は CRuby と同じだったりします。
「Rubyソースコード完全解説」@<bib>{rhg} という素晴らしい書籍に詳細が掲載されていますので、 CRuby と共通している部分の解説は省かせていただこうと思います。
また、最近ですと「Ruby Under a Microscope」@<bib>{under_microscope} @<bib>{under_microscope_ja} という本で1.8以降のRuby実装に関する解説がしてあります。
詳しく知りたい方はそちらも合わせてご参照ください。

ざっくり説明しますと、 parser_yylex() は nextc() でソースコードを少しずつ読み出し、 switch 文でトークンを識別し、トークンの内容を返します。
またキーワードの検出については lex.def で定義される mrb_reserved_word() を呼んでいます。
yyparse() は yacc の BNF によるシンタックスの定義から自動生成されます。

=== コード生成

コード生成を行っているのは codegen.c となります。
このソースコードにおいて読解のエントリポイントとなるのは mrb_generate_code() です。
この関数は mrb_state （詳細は RiteVM の節で触れます）と parser_state （mruby コンパイラの状態を管理するためのデータ構造）型の引数を取ってコード生成を行い、その結果を RProc の形式で返却します。
mrb_generate_code() を追っていきますと、大筋としては codegen() で irep の生成を行い、その結果を用いて mrb_proc_new() で RProc オブジェクトを生成しているのが分かります。

mrb_generate_code() は引数に渡された parser_state から構文木のルートノードへのポインタを取り出して codegen() に渡します。
構文木はリストになっており、先頭要素を取り出すメンバ car と続くリストを取り出すメンバ cdr を持ちます。
codegen() では基本的に car で取り出したノードのタイプによって switch 文で分岐し、コード生成を行って iseq の末尾に追加していきます。

ノードのタイプは 103 種類ほどあり、 switch 文で分岐した先のコード生成について逐一詳細を追っていくのは困難です。
ここでは @<list>{hello.rb} のような典型的な "Hello,World!" と標準出力するだけの mruby スクリプトの構文木を確認し、その構成要素について追ってみます。
なお、ここで生成されるコードに関しての詳細は RiteVM の節で触れていきます。

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
それが終わると OP_STOP 命令もしくは OP_RETURN 命令を追加します。
ちなみにコード生成は MKOP_A などのマクロによって生成されます。コード生成マクロは取りうるオペランドによってバリエーションがあります。
そして生成されたコードの iseq への追加は genop() で行われます。

次は NODE_BEGIN です。
これはツリーが辿れない場合は NODENIL 命令を追加、辿れる場合は辿れる分まで codegen() を while ループで呼び出してコード生成を行います。

NODE_CALL の処理は gen_call() にまとめられています。
gen_call() ではまずリストの次の要素を codegen() に渡します。
これがメソッドのレシーバとなる想定であり、上記コードなら NODE_SELF がそれに該当します。
その後引数の個数の判定を行い、呼び出すメソッドが演算子であれば OP_ADD など、演算子ではなくブロックを渡さなければ OP_SEND 命令、ブロックを使うなら OP_SENDB 命令を追加します。

NODE_SELF はシンプルで、 genop() で OP_LOADSELF 命令を追加するだけです。

最後に NODE_STR ですが、まずリストの後続 2 要素を取り出します。
これらはそれぞれ char* 型の文字列先頭へのポインタと文字列のサイズを意味する値が入っています。
その後 mrb_str_new() を使って、この 2 変数から String 型オブジェクトを生成し、 new_lit() に渡します。
new_lit() は渡されたオブジェクトを irep の pool 領域に格納します。
ちなみにこの new_lit() は文字列の他にも整数、浮動小数点数リテラルから生成したオブジェクトを保存するのにも使用します。


== RiteVM

前節で触れた irep は、実際には mruby においてどのように解釈されるのでしょうか？
mruby で irep を実行する VM 、 RiteVM についてつらつらと書いていきます。

=== 命令セットアーキテクチャ

RiteVM はレジスタマシンとして動作します。
1 つの命令は 32 ビットで表現され、 7 ビットのオペコードと 0 個以上 3 個以下のオペランドを取る形式になり、 @<table>{operands} のようなバリエーションが存在します。

//table[operands][オペランド種別]{
名前	内容 
-------------------------------------------------------------
A	9 ビット長のオペランド
Ax	25 ビット長のオペランド
B	9 ビット長のオペランド
Bx	16 ビット長のオペランド
sBx	16 ビット長のオペランド(符号付き)
C	7 ビット長のオペランド
//}

mruby の一部の命令をざっくりとカテゴリ分けしたものを @<table>{opcode_loads},  @<table>{opcode_variables},  @<table>{opcode_controlls},  @<table>{opcode_operations},  @<table>{opcode_objects},  @<table>{opcode_etc} に記します。
なおそれぞれの表中の、命令の内容説明で R(A) などと書かれている部分は オペランド A 番目のレジスタの値、 Syms(A) の場合は syms の A 番目の値、 Pool(A) の場合は pool の A 番目の値だと解釈してください。

捕捉となりますが、二項演算命令ではレジスタの値の型が数値、浮動小数点数、文字列かどうかにより結果が変わります。
これらの条件にマッチしない場合は代わりに OP_SEND 命令を実行します。

//table[opcode_loads][基本ロード命令]{
命令	内容
-------------------------------------------------------------
OP_MOVE A B	R(A) に R(B) の値をコピーする
OP_LOADL A Bx	R(A) に Pool(Bx) の値をコピーする
OP_LOADI A sBx	R(A) に sBx の値をコピーする
OP_LOADSYM A Bx	R(A) に Syms(Bx) をコピーする
OP_LOADSELF A	R(A) に self の値をコピーする
OP_LOADNIL A	R(A) に nil をコピーする
//}

//tsize[40,80]
//table[opcode_variables][変数操作命令]{
命令	内容
-------------------------------------------------------------
OP_GETIV A Bx	R(A) に Syms(Bx) が指す self のインスタンス変数の値をコピーする
OP_SETIV A Bx	R(A) の値を Syms(Bx) が指す self のインスタンス変数の値として格納する
OP_GETCONST A Bx	R(A) に Syms(Bx) が指す定数の値をコピーする
OP_SETCONST A Bx	R(A) の値を Syms(Bx) が指す定数の値として格納する
//}

//tsize[30,100]
//table[opcode_controlls][制御命令]{
命令	内容
-------------------------------------------------------------
OP_JMP sBx	sBx 分だけプログラムカウンタを増やす
JMPIF, JMPNOT A sBx	R(A) の評価結果が true/false だった場合、 sBx 分だけプログラムカウンタを増やす
OP_ONERR sBx	プログラムカウンタに sBx 加算した値を rescue 時処理として実行コンテキストに追加する
OP_RESCUE A	R(A) に mrb->exc をコピーし、 mrb->exc をクリアする
OP_POPERR A	実行コンテキストから A の値分だけ rescue 時処理を削除する
OP_RAISE A	R(A) を proc オブジェクトとして解釈して mrb->exc にコピーし、例外処理を開始する
OP_EPUSH Bx	Bx 番目の reps を ensure 処理すべき proc オブジェクトとして mrb->c->ensure に格納する
OP_POPERR A	実行コンテキストから A の値分だけ ensure 時処理を呼び出しつつ削除する
OP_SEND A B C	R(A) のオブジェクトをレシーバとし、 Syms(B) のメソッドを C の個数の引数で呼び出す
OP_ENTER Ax	メソッドの引数のチェックを行う。 Ax から 5:5:1:5:5:1:1 ビットのフィールドを取り出し、引数の数情報として利用する
OP_RETURN A B	 R(A) を戻り値としてメソッドからリターンする
//}

//tsize[30,100]
//table[opcode_operations][二項演算命令]{
命令	内容
-------------------------------------------------------------
OP_ADD A B C	R(A) と R(A+1) の加算結果を R(A) に格納する
OP_SUB A B C	R(A) と R(A+1) の減算結果を R(A) に格納する
OP_MUL A B C	R(A) と R(A+1) の乗算結果を R(A) に格納する
OP_DIV A B C	R(A) と R(A+1) の除算結果を R(A) に格納する
OP_EQ A B C	R(A) と R(A+1) を比較して同値かどうかをを R(A) に格納する
OP_LT A B C	R(A) と R(A+1) を比較して後者が大きい値かどうかを R(A) に格納する
OP_GT A B C	R(A) と R(A+1) を比較して後者が小さい値かどうかを R(A) に格納する
//}

//tsize[40,90]
//table[opcode_objects][オブジェクト操作命令]{
命令	内容
-------------------------------------------------------------
OP_ARRAY A B C	R(B) から R(B+C) に格納されている値をコピーして Array オブジェクトを R(A) に格納する
OP_STRING A Bx	Pool(Bx) に格納された String オブジェクトを R(A) にコピーする
OP_HASH A B C	R(B) から R(B+C*2) に格納されているオブジェクトをキーとバリューのペアの繰り返しとみなして読み出し、 Hash オブジェクトを生成して、その結果を R(A) に格納する
OP_CLASS A B	新しいクラスを定義する。 R(A) が親クラス、R(A+1) が super、 Syms(B) がクラス名になる。定義後に、オブジェクトを生成して R(A) に格納する
OP_METHOD A B	メソッドを定義する。R(A) がクラス、 R(A+1) がメソッドの中身となる proc オブジェクト、 Syms(B) がメソッド名になる
//}

//tsize[30,80]
//table[opcode_etc][その他の命令]{
命令	内容
-------------------------------------------------------------
OP_NOP	なにもしない
OP_STOP	VM の実行を終了する
//}

=== 主なデータ構造

RiteVM では命令の実行状態を管理するため mrb_state や mrb_context 、 mrb_callinfo などといったデータ構造を用意しています。
RiteVM の命令実行は irep で表現された命令を解釈しながらこれらのデータを操作していくことで進められていきます。
各データ構造の関係をざっくり示すと @<img>{mrb_states} のようになります。

//image[mrb_states][RiteVM の状態管理のためのデータ構造][scale=0.7]

もう少し詳細に踏み込んでみましょう。

==== RiteVM の状態を示すデータ構造 mrb_state 

mrb_state は RiteVM の実行状態情報を保持するデータ構造です。
@<table>{mrb_state} のような情報を保持します。

//table[mrb_state][mrb_state の構造]{
名前	内容 
-------------------------------------------------------------
allocf	メモリ割り当て関数。デフォルトでは realloc() を使う
c, root_c	実行コンテキスト
exc	例外ハンドラへのポインタ
globals	グローバル変数テーブル
*_class	Object や Class, String など基本クラスへのポインタ
gc	GC に関する情報
ud	補助データ。 mrbgem の開発者が VM 単位で管理したいデータを格納するのに使ったりする
//}

==== 実行コンテキスト mrb_context

mrb_context は RiteVM の実行コンテキストを保持するデータ構造です。
この構造は Fiber を実現するために使われています。
始点となるルート実行コンテキスト（mrb_state の root_c）から、 fiber を生成するたびにそれに対応する実行コンテキストが生成されるイメージです。

mrb_context では @<table>{mrb_context} のような情報を保持します。

//tsize[40,80]
//table[mrb_context][mrb_context の構造]{
名前	内容 
-------------------------------------------------------------
prev	自身を呼び出した側の実行コンテキスト。 root_c か親 fiber になる
stack, stbase, stend	RiteVM のレジスタ情報
ci, cibase, ciend	メソッド呼び出し情報
rescue, rsize	rescue ハンドラに関する情報
ensure, esize	ensure ハンドラに関する情報
status, fib	そのコンテキストの持ち主の fiber の実行状態とその fiber へのポインタ
//}

stack というメンバが出てきて混乱を招きそうですが、これは RiteVM がレジスタとして使っている領域になります。
メソッドを呼び出す際には新たにスタックを積んでレシーバオブジェクトや引数、ブロックをセットし、メソッドから戻る際は戻り値を元々レシーバオブジェクトが存在した領域に格納します。

//image[stack][stack 領域の使われ方][scale=0.8]

Fiber の中身についても少しだけ触れてみます。
Fiber は @<table>{RFiber} に示す通り、実行コンテキストを保持するオブジェクトです。
ソースコード中では RFiber という構造体で表現されています。

//tsize[40,80]
//table[RFiber][RFiber の構造]{
名前	内容 
-------------------------------------------------------------
MRB_OBJECT_HEADER	オブジェクトのヘッダ
cxt	その Fiber オブジェクトの実行コンテキスト
//}

ちなみに mruby では Fiber の実装は mrbgems/mruby-fiber/ 以下に存在し、 mrbgem として切り出されています。
つまりは不要なら Fiber を含まない形でビルドできる訳ですね！

==== メソッドコールスタック mrb_callinfo

mrb_callinfo は mruby のメソッドのコールスタックを管理するための構造です。
メソッドが呼び出される度に mrb_context 構造体の ci に新しい mrb_callinfo 構造体データを push 、逆にメソッドから抜ける際は ci の要素を pop します。

@<table>{mrb_callinfo} のような情報を保持します。

//tsize[30,80]
//table[mrb_callinfo][mrb_callinfo の構造]{
名前	内容 
-------------------------------------------------------------
proc	呼び出されたメソッドの Proc オブジェクト
nregs	レジスタ数
ridx, eidx	rescue, ensure 呼び出しのネストの深さ
env	レジスタやシンボル情報を保存する REnv 構造体データ
//}


=== RiteVM の実装を追う

==== RiteVM のメイン処理

まずは include/mruby.h で宣言されている下記の関数をエントリポイントとして処理を追っていきます。

 * mrb_run()
 * mrb_toplevel_run_keep()
 * mrb_toplevel_run()

この 3 つの関数は、 mrb_state と RProc をとり RiteVM の実行を開始します。
複雑な処理はしておらず、メインの処理は mrb_context_run() に任せています。
というわけで次は mrb_context_run() を覗いてみましょう。
この関数は行数が多いですが、行っていることはシンプルです。
大部分の処理のフローは 1) オペコードをフェッチ 2) switch 文でオペコードを判別 3) 命令の実行 を for 文でループする形になります。

==== 初期化処理

まず命令処理のループに入る前に初期化処理を行います。
ここで mrb_context_run() の引数の mrb から stack メンバを取り出し（mrb->c->stack）、レジスタとして扱います。
また mrb->c->ciの proc と ngres をそれぞれ設定します。

実行する命令は proc のメンバの irep->iseq から取り出します。
この時の ireq->iseq の先頭要素へのポインタはプログラムカウンタとして持っておきます。

==== RiteVM 命令処理

先述の通り命令の処理は巨大な switch 文を for ループで回すような実装になっています。
大枠としては @<list>{mrb_context_run} に示すような内容となります。

//listnum[mrb_context_run][mrb_context_run() のざっくり内容][c]{
MRB_API mrb_value
mrb_context_run(mrb_state *mrb, struct RProc *proc, ...)
{
  mrb_irep *irep = proc->body.irep;
  mrb_code *pc = irep->iseq;
  // ... こんな感じでいろいろ変数初期化 ...

  INIT_DISPATCH {
    CASE(OP_NOP) {
      // ... OP_NOP の処理 ...
      NEXT;
    }
    CASE(OP_MOVE) {
      // ... OP_MOVE の処理 ...
      NEXT;
    }
    ...
  }
  // ... 終了処理 ...
}
//}

まずは INIT_DISPATCH マクロでプログラムカウンタを更新し、 switch 文による分岐を行います。
次に各命令に対応した case（CASE というマクロに包まれています） で命令処理を行います。
各命令の実装を紹介していくとキリがないので詳細は省きますが、概ね GETARG_A() などのマクロを用いてオペランドを取り出し、その値から使用するレジスタ、 pools や syms の値を特定し、操作を行います。
例えば OP_MOVE 命令は GETARG_A() と GETARG_B() で A と B を取り出し、それらをインデックスとしてレジスタの値を参照し、代入文を実行します。

OP_SEND や OP_RETURN などによって実現されるメソッド呼び出しやリターンについては少々複雑になっています。
基本的にはこれまで紹介してきた mrb->c->stack や mrb->c->ci 、プログラムカウンタを操作します。
mrb->c->stack は領域が足りなくなったら stack_extend() で拡張します。
また mrb->c->ci の push / pop のため cipush() / cipop() を呼び出します。

==== RiteVM による実際の irep の解釈例

これだけでは RiteVM の処理のイメージが見えてこないと思われるので、実際の irep の処理を確認してみましょう。
コード生成部分で触れた、単純な Hello,World! スクリプトから生成される irep は下記の通りになります。

//cmd{
irep 0x7fdba9c0b9b0 nregs=4 nlocals=1 pools=1 syms=1 reps=0
file: hello.rb
    1 000 OP_LOADSELF   R1
    1 001 OP_STRING     R2      L(0)    ; "Hello,World!"
    1 002 OP_SEND       R1      :puts   1
    1 003 OP_STOP
//}

今回生成された irep は、元のスクリプトがひとつのスコープしかない都合 1 個であり、他の irep を参照しません（reps=0）。
また、 pools の要素は "Hello,World!" を指す String オブジェクトの分に 1 個（pools=1）、 syms の数は puts メソッドの名前のため 1 個（syms=1）のようです。
レジスタ数はコード中に明示的に参照している 2 個に合わせて、レシーバオブジェクトとブロックの管理のために更に 2 個要するので合計 4 個（nregs=4）になります。

irep が示す命令の処理フローを追ってみます。
まず OP_LOADSELF ですが単純に R1 に self を代入します。
ここでの self はスコープの一番外側である都合、 mrb_top_self() が返す値が入る形になります。
次の OP_STRING は pools に格納された 0 番目の要素、 "Hello,World!" という String オブジェクトを取り出し R2 に格納します。
その後 OP_SEND で puts メソッドを呼び出します。
puts メソッドから見て self は R1 、引数は 1 個でその値は R2 になります。
最後にすべての命令を実行し終えたので OP_STOP で終了処理に入ります。

==== 例外処理の詳細

例外処理の実装は複雑なので、別途 irep を出力してそれを足がかりに追ってみます。
まずは @<list>{exc.rb} に示すようなシンプルな例外処理を行うスクリプトを書いてみます。

//listnum[exc.rb][例外処理するスクリプト例][ruby]{
begin
  raise StandardError
rescue
  puts "rescue"
ensure
  puts "ensure"
end
//}

このスクリプトから生成される irep に、ざっくりとした処理のフローを加えたものが @<img>{exc} のようになります。

//image[exc][例外処理を含む irep とその処理フロー]

では具体的な各処理のフローを深掘りしてみましょう。
1 番目の irep は exc メソッドの中を表しているように見えます。
早速 OP_EPUSH が呼び出され、 ensure ブロックを表現する irep（2 番目の irep）を mrb->c->ensure に追加し、 mrb->c->ci->eidx をインクリメントします。
また同様に rescue ブロックに関しても OP_ONERR で mrb->c->resuce に追加し、 mrb->c->ci->ridx をインクリメントします。
ただし OP_ONERR は irep を追加するのではなく、同一 irep 内の iseq のポインタを格納する形になります。

その後 OP_SEND で raise メソッドを呼び出します。
このメソッドは Kernel#raise として src/kernel.c に定義され、 C の関数としては mrb_f_raise() が該当します。
この関数は引数により取りうる振る舞いが変わりますが、最終的に src/error.c に定義される mrb_exc_raise() を呼び出します。
mrb_exc_raise() では mrb->exc に例外オブジェクトを格納し、 MRB_THROW() マクロで例外と rescue ブロックの捕捉を行います。

ここで mrb_context_run() における例外処理の実現方法について触れておきます。
ざっくりと C のコードで表現すると @<list>{mrb_context_run_exc.c} のようになります。

//listnum[mrb_context_run_exc.c][mrb_context_run() における例外処理][c]{
MRB_TRY(...) {
  // MRB_THROW を呼ぶ可能性のある処理
} MRB_CATCH(...) {
  // MRB_THROW が呼ばれた際に実行する処理
  // mrb_context_run() においては例外捕捉フラグを立てて再度 MRB_TRY のブロックに入る
}
//}

mrb_context_run() では命令を処理する for ループを MRB_TRY マクロに続くブロックで囲います。
この MRB_TRY が setjmp マクロを使って例外が上がった際にジャンプする先として扱われます。
MRB_TRY のブロック内で MRB_THROW マクロが呼び出されると、これが longjmp() を呼び出して MRB_TRY まで戻ってくることになり、かつ MRB_TRY 内の if 文の条件が満たせなくなる戻り値を返すため MRB_CATCH マクロのブロックが実行されます。
MRB_CATCH マクロのブロックでは例外の捕捉を行なう旨のフラグが true になり、これにより実際に例外処理をする L_RAISE ラベルまでジャンプします。

そんな流れで L_RAISE ラベルにジャンプした後ですが、 mrb->exc と mrb->c->ci->ridx がセットされている都合 rescue の処理が発生すると判断され、 L_RESCUE ラベルにジャンプします。
L_RESCUE では mrb->c->rescue から rescue 時にジャンプする先のポインタ、この例では 006 の箇所へジャンプします。

006 へジャンプした後は OP_RESCUE により、 raise メソッドによって設定された mrb->exc を R1 にコピーして mbr->exc はゼロクリアします。
その後 StandardError を R2 に、 R1 にコピーした mrb->exc を R3 にコピーし、 R2 をレシーバオブジェクトとして === メソッドを OP_SEND で呼び出します。
これが、例外クラスが rescue の対象のクラス（ここでは StandardError ）と同値かの比較に相当します。
この例ではこの比較によって R2 に true が格納され、後続の OP_JMPIF の条件にマッチし 012 へジャンプします。

012 から 014 は 単純に puts メソッドに文字列を出力させる命令群です。
これらについては既に触れたとおりなので割愛します。
puts メソッドの呼び出しまで完了したら OP_JMP で 018 にジャンプします。

018 の OP_EPOP では A の回数だけ mrb->c->ensure に詰まれた proc を呼び出します。
この例では A に 1 が指定され、かつ冒頭で OP_EPUSH で ensure の proc が 1 つ詰まれているのでそれを呼び出すことになります。
実際の ensure proc 呼び出しは ecall() という関数に切り出されています。
ecall() は mrb->c->ensure から proc を取り出して、 cipush() などで新しいコールスタックと mbr->c->stack を準備し、 mrb_run() で ensure の命令が収められている irep を実行します。

ensure proc の irep の内容や OP_STOP についてもこれまでで解説済みなので割愛します。
・・・以上で、 raise から始まり rescue で例外をキャッチしてから ensure が呼び出されるまでのフローです。

余談ですが、 mrb->c->rescue や mrb->c->ensure のサイズが足りなくなった際は、最初は 16 要素分、その後不足が出た際に 32, 64 と倍々に mrb_realloc() で領域を確保し直します。
この倍々に領域を増やすというロジックは mruby の実装ではよく見られるパターンでして、具体的には mrb->c->stack や mrb->c->ci の伸長でも同様だったりします。

