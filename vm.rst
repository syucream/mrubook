mruby Virtual Machine(RiteVM)
#############################

Overview
*********

* http://www.dzeta.jp/~junjis/code_reading/index.php?mruby%2F%A5%B3%A1%BC%A5%C9%BC%C2%B9%D4%A4%F2%C6%C9%A4%E0 とか参考になりそう。

Details
**************

* レジスタマシンのようだ
* レジスタは mrb_context で保持する stack に積んであるようだ

  - mrb_vm_exec() では regs マクロでレジスタを取り出す

* TODO: どのくらいの種類のレジスタがあるのか確認

RiteVM の機械語
===============

* mrb_code (uint32_t の typedef) で表現される

オペランド
==========

* 0 個以上 3 個以下とる
* オペランドについては A, B, C という名前になる
* オペランド修飾子

  - なにも付かない場合: 9 ビットサイズになる
  - オペランドの前にs: signed, 符号付き
  - オペランドの後ろに x: (25 - 他のオペランドサイズ)の値になる
  - オペランドの後ろに z: よーわからん

* めも: 9 ビットとかってなんだろう？どこか符号ビットにしているのか？

オペコード
==========

* 0 個以上 3 個以下のオペランドをとる
* mruby/opecode.h に enum で定義されている

  - 各オペコードに対応する VM における処理は vm.c の mrb_vm_exec() を読むこと

特殊
----

* OP_NOP

  - なにもしない

ロード系
--------

* OP_MOVE A B

  - B を A にコピーする

* OP_LOADL A Bx

  - Bx のプールの値を A にコピーする????

* OP_LOADI A sBx

  - I は Int?
  - sBx の値を Int 型とみなして A にコピーする???

* OP_LOADSYM A Bx

  - SYM は symbol のはず
  - Bx の値を Symbol 型とみなして A にコピーする???

* OP_LOADNIL A

  - A に nil をコピーする

* OP_LOADSELF A

  - A に self をコピーする
  - self は regs[0] に相当する

* OP_LOADT A

  - A に true をコピーする

* OP_LOADF A

  - A に false をコピーする


制御系
------

* OP_JMP sBx

  - sBx へ jump する
  - pc を sBx の値に書き換えて break (JUMP マクロ)

* OP_JMPIF A sBx

  - もし A の値が true なら sBx へ jump する
  - A の評価には mrb_test マクロを使う

* OP_JMPNOT A sBx

  - OP_JMPIF の逆バージョン
  - A の値が false なら sBx へ jump する

...

mrb_irep (instruction or intermediate representation の略？)
============================================================

* ローカル変数

  - nlocals にローカル変数の数が格納される
  - 16 ビットに収まる範囲であればローカル変数保持できるのか？

* レジスタ

  - nregs にレジスタ数が格納される
  - 16 ビットに(ry

* フラグ

  - irep の解釈の仕方を制御するフラグ
  - FLAG_BYTEORDER_BIG
    
    * ビッグエンディアンフラグ
    * bin_to_uint32() でビッグエンディアンとして iseq を解釈

  - FLAG_BYTEORDER_LIL

    * リトルエンディアンフラグ
    * bin_to_uint32l() でリトルエンディアンとして iseq を解釈

  - FLAG_BYTEORDER_NATIVE
    
    * ネイティブのエンディアンに従うフラグ
    * memcpy で iseq を一気にコピー

  - FLAG_SRC_MALLOC

    * バイトコード読み出しの際に mrb_malloc した領域に解釈した内容をコピーする

  - FLAG_SRC_STATIC

    * バイトコード読み出しの際にコピーせず、元のバイトコードへのポインタを保持する

* iseq

  - コードセグメント（実体は mrb_code の配列）の先頭番地へのポインタ
  - RiteVM に処理させたい際は、 iseq を pc に突っ込んで開始する

* pool
* syms

  - mrb_sym の配列

* reps

  - 続きの？ mrb_irep のリスト

.. code :: c

  typedef struct mrb_irep {
    uint16_t nlocals;
    uint16_t nregs;
    uint8_t flags;
  
    mrb_code \*iseq;
    mrb_value \*pool;
    mrb_sym \*syms;
    struct mrb_irep \*\*reps;
  
    struct mrb_locals \*lv;

    const char \*filename;
    uint16_t \*lines;
    struct mrb_irep_debug_info\* debug_info;
  
    size_t ilen, plen, slen, rlen, refcnt;
  } mrb_irep;

mruby 実行形式
==============

* mruby コマンドってやつ。コンパイル前のスクリプト .rb 、もしくはコンパイル済バイトコード .mrb を実行する

  - mrbgems/mruby-bin-mruby/ にコードあり

* mrb_load_irep_file_cxt() などを読んでバイトコードを読み込み、実行

  - 実行処理は mrb_top_run() を呼ぶことで行う

mruby コアデータ構造
==========================

* RiteVM の実行状態や実行対象の手続きに関していくつものデータ構造が存在する

mrb_state
---------

* http://qiita.com/miura1729/items/822a18051e8a97244dc3 が参考になりそう。

* C で mrbgem を実装しようとするとちらほら目にする構造体
* mruby の VM の状態を保持
* 各基本クラスへのポインタやGC情報、グローバル変数などを格納する

* RiteVM の実行コンテキストをもつ

.. code :: c

  struct mrb_context \*c;
  struct mrb_context \*root_c;

* mruby 組み込みクラスの定義へのポインタを持つ

  - これらはほぼ全て（全部じゃないよね？） init された後は書き換わることは無い
  - string.c とか init 関数毎に代入箇所が散らばっているので注意！
  
.. code :: c

  struct RClass \*object_class;
  struct RClass \*class_class;
  struct RClass \*module_class;
  struct RClass \*proc_class;
  struct RClass \*string_class;
  struct RClass \*array_class;
  struct RClass \*hash_class;

  struct RClass \*float_class;
  struct RClass \*fixnum_class;
  struct RClass \*true_class;
  struct RClass \*false_class;
  struct RClass \*nil_class;
  struct RClass \*symbol_class;
  struct RClass \*kernel_module;

mrb_context
------------

* prev

  - 以前のコンテキスト
  - 例えば Fiber における親 fiber のコンテキスト
  - 例えば ... ほかにある？

* ...

* status

  - そのコンテキストの持ち主となる fiber の実行状態
  - 詳しくは Fiber の項目を参考

* fib

  - そのコンテキストの持ち主となる fiber へのポインタ
  - そもそも fiber で実行していない場合...どうなる？ NULL ？

.. code :: c

  struct mrb_context {
    struct mrb_context \*prev;
  
    mrb_value \*stack;
    mrb_value \*stbase, \*stend;
  
    mrb_callinfo \*ci;
    mrb_callinfo \*cibase, \*ciend;
  
    mrb_code \*\*rescue;
    int rsize;
    struct RProc \*\*ensure;
    int esize;
  
    enum mrb_fiber_state status;
    mrb_bool vmexec;
    struct RFiber \*fib;
  };

mrb_callinfo
------------

* メソッド呼び出しに関する情報を保持？
* 与えられた引数の数など

RProc
------

* mruby の Proc オブジェクト型
* このオブジェクトを VM で実行して mruby で処理を行うイメージ

read_irep()
-----------------

* バイトコードを読み出して、 mrb_irep の形式に当てはめる

  - まず read_binary_header() でヘッダを読み出す
  - 次の処理は各セクションを探して read_section_*() 関数で irep に変換していく

read_section_irep()
---------------------

* irep セクションを読み出す

  - mrb_irep のうち重要なメンバが読み出される
  - irep セクションは必須のものになる。あとはオプションのはず

read_section_lineno()
---------------------

* lineno セクションを読み出す

  - mrb_irep のうち filename, lines が読み出される

read_section_debug()
---------------------

* debug セクションを読み出す

  - mrb_irep のうち debug_info が読み出される

read_section_lv()
---------------------

* lv セクションを読み出す

  - mrb_irep のうち lv が読み出される

RiteVM バイトコードフォーマット
--------------------------------

* irep として解釈するバイトコードは下記のようなフォーマットになってる？

::

  +---------------------+
  | binary header       |
  +---------------------+
  | irep section header |
  +---------------------+
  | irep section body   |
  +---------------------+
  | any* section header |
  +---------------------+
  | any* section body   |
  +---------------------+
  *: irep, lineno, debug or lv

RiteVM バイトコードのヘッダ
----------------------------

* rite_binary_header 構造体で表現される
* フォーマットは下記の通り:

  - 4 バイト: バイナリ ID。基本的に "RITE" という 4 文字が書かれる
  - 4 バイト: バイナリバージョン。本文書の執筆段階では "0003"
  - 2 バイト: バイナリの CRC
  - 4 バイト: バイナリサイズ
  - 4 バイト: コンパイラ名。 mrbc でビルドした場合は "MATZ"
  - 4 バイト: コンパイラバージョン

APIs
*******

スクリプトの実行
==================

mrb_run()
----------------

* 渡された mrb_state 上で RProc を実行する
* 実際には mrb_vm_run() を呼んでいるだけ

mrb_top_run()
-------------

* 渡された mrb_state 上で RProc を実行する
* mrb_run() より色々前準備をする場合がある

  - が、いまいちよくわからん。あとでよむ


mrb_vm_run()
----------------

* stack_extend() でレジスタの置き場所をスタック上に（必要な領域になるように）作ってる？
* スタックに積んだレジスタ R0 (って呼ぶべきなのか？) に self を格納しておく
* mrb_vm_exec() を読んで VM のメインループ開始

  - 最初の処理開始位置である引数の pc に irep->iseq を渡している TODO: irep について読む

mrb_vm_exec()
----------------

* INIT_DISPATCH マクロ

  - pc, プログラムカウンタから次に解釈すべき命令を fetch する
  - CODE_FETCH_HOOK マクロを実行。なにこれ？
  - switch 文でオペコード毎の処理に分岐

スクリプトの実行
==================

mrb_value mrb_load_irep_cxt(mrb_state\*, const uint8_t\*, mrbc_context\*)
------------------------------------------------------------------------------

* バイトコードを読み、 irep に変換して実行する

  - mrb_read_irep() でバイトコードから irep を生成する
  - 生成した irep から RProc を生成する
  - 実行処理は mrb_top_run() を呼び出すことで行う

mrb_value mrb_load_irep(mrb_state\*, const uint8_t\*)
------------------------------------------------------

* mrb_load_irep_cxt() を第三引数 NULL で呼び出す

ダンプ
==================

int mrb_dump_irep(mrb_state \*mrb, mrb_irep \*irep, uint8_t flags, uint8_t \*\*bin, size_t \*bin_size)
----------------------------------------------------------------------------------------------------------

* mrb_irep をバイトコードに dump する

  - 第 2 引数に dump 対象の mrb_irep を指定する
  - 第 3 引数は dump 結果のフォーマットを指定

    * DUMP_DEBUG_INFO: デバッグ情報を出力する
    * DUMP_ENDIAN_BIG: ビッグエンディアンで出力
    * DUMP_ENDIAN_LIL: リトルエンディアンで出力
    * DUMP_ENDIAN_NAT: ネイティブのエンディアンで出力
    * DUMP_ENDIAN_MASK: 上記 3 フラグを取り出す際のビットマスク

  - 第 4,5 引数に dump 結果のバイトコードがセットされる。これらは内部的に mrb_malloc() で領域確保がされる
