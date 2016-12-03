mruby Virtual Machine(RiteVM)
#############################

Overview
*********

* http://www.dzeta.jp/~junjis/code_reading/index.php?mruby%2F%A5%B3%A1%BC%A5%C9%BC%C2%B9%D4%A4%F2%C6%C9%A4%E0 とか参考になりそう。
* 素朴な疑問: RiteVM という名前はまだ生きているのか？？

Details
**************

* レジスタマシンのようだ
* レジスタは mrb_context で保持する stack に積んであるようだ

  - mrb_vm_exec() では regs マクロでレジスタを取り出す

* TODO: どのくらいの種類のレジスタがあるのか確認

RiteVM の機械語
===============

* mrb_code (uint32_t の typedef) で表現される

  - RISC ... と言えるのかな？

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

mrb_irep (instruction? intermediate? representation の略？)
============================================================

* XXX: この記述は後で別ファイルに移動するかも

* おそらく RiteVM に解釈させる機械語プログラムを表現したもの・・・なんだと思われる

* ローカル変数

  - nlocals にローカル変数の数が格納される
  - 16 ビットに収まる範囲であればローカル変数保持できるのか？

* レジスタ

  - nregs にレジスタ数が格納される
  - 16 ビットに(ry

* フラグ

  - 何に使ってるのか不明

* iseq: コードセグメントの先頭番地へのポインタ

  - RiteVM に処理させたい際は、 iseq を pc に突っ込んで開始する

* 定義については下記の通り

.. code :: c

  /* Program data array struct */
  typedef struct mrb_irep {
    uint16_t nlocals;        /* Number of local variables */
    uint16_t nregs;          /* Number of register variables */
    uint8_t flags;
  
    mrb_code *iseq;
    mrb_value *pool;
    mrb_sym *syms;
    struct mrb_irep **reps;
  
    struct mrb_locals *lv;
    /* debug info */
    const char *filename;
    uint16_t *lines;
    struct mrb_irep_debug_info* debug_info;
  
    size_t ilen, plen, slen, rlen, refcnt;
  } mrb_irep;

...

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

  struct mrb_context *c;
  struct mrb_context *root_c;

* mruby 組み込みクラスの定義へのポインタを持つ

  - これらはほぼ全て（全部じゃないよね？） init された後は書き換わることは無い
  - string.c とか init 関数毎に代入箇所が散らばっているので注意！
  
.. code :: c

  struct RClass *object_class;            /* Object class */
  struct RClass *class_class;
  struct RClass *module_class;
  struct RClass *proc_class;
  struct RClass *string_class;
  struct RClass *array_class;
  struct RClass *hash_class;

  struct RClass *float_class;
  struct RClass *fixnum_class;
  struct RClass *true_class;
  struct RClass *false_class;
  struct RClass *nil_class;
  struct RClass *symbol_class;
  struct RClass *kernel_module;

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
    struct mrb_context *prev;
  
    mrb_value *stack;                       /* stack of virtual machine */
    mrb_value *stbase, *stend;
  
    mrb_callinfo *ci;
    mrb_callinfo *cibase, *ciend;
  
    mrb_code **rescue;                      /* exception handler stack */
    int rsize;
    struct RProc **ensure;                  /* ensure handler stack */
    int esize;
  
    enum mrb_fiber_state status;
    mrb_bool vmexec;
    struct RFiber *fib;
  };

mrb_callinfo
------------

* メソッド呼び出しに関する情報を保持？
* 与えられた引数の数など

RProc
------

* mruby の Proc オブジェクト型
* このオブジェクトを VM で実行して mruby で処理を行うイメージ

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

