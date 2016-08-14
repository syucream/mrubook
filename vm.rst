mruby Virtual Machine(RiteVM)
#############################

* http://www.dzeta.jp/~junjis/code_reading/index.php?mruby%2F%A5%B3%A1%BC%A5%C9%BC%C2%B9%D4%A4%F2%C6%C9%A4%E0 とか参考になりそう。
* 素朴な疑問: RiteVM という名前はまだ生きているのか？？

スクリプトの実行
******************

mrb_run()
================

* 渡された mrb_state 上で RProc を実行する
* 実際には mrb_vm_run() を呼んでいるだけ

mrb_top_run()
=============

* 渡された mrb_state 上で RProc を実行する
* mrb_run() より色々前準備をする場合がある

  - が、いまいちよくわからん。あとでよむ


mrb_vm_run()
================

* stack_extend() でレジスタの置き場所をスタック上に（必要な領域になるように）作ってる？
* スタックに積んだレジスタ R0 (って呼ぶべきなのか？) に self を格納しておく
* mrb_vm_exec() を読んで VM のメインループ開始

  - 最初の処理開始位置である引数の pc に irep->iseq を渡している TODO: irep について読む

mrb_vm_exec()
================

* INIT_DISPATCH マクロ

  - pc, プログラムカウンタから次に解釈すべき命令を fetch する
  - CODE_FETCH_HOOK マクロを実行。なにこれ？
  - switch 文でオペコード毎の処理に分岐

アーキテクチャ
**************

* レジスタマシンのようだ
* レジスタは mrb_context で保持する stack に積んであるようだ

  - mrb_vm_exec() では regs マクロでレジスタを取り出す

* TODO: どのくらいの種類のレジスタがあるのか確認

RiteVM の機械語
***************

* mrb_code (uint32_t の typedef) で表現される

  - RISC ... と言えるのかな？

オペランド
**********

* 0 個以上 3 個以下とる
* オペランドについては A, B, C という名前になる
* オペランド修飾子

  - なにも付かない場合: 9 ビットサイズになる
  - オペランドの前にs: signed, 符号付き
  - オペランドの後ろに x: (25 - 他のオペランドサイズ)の値になる
  - オペランドの後ろに z: よーわからん

* めも: 9 ビットとかってなんだろう？どこか符号ビットにしているのか？

オペコード
**********

* 0 個以上 3 個以下のオペランドをとる
* mruby/opecode.h に enum で定義されている

  - 各オペコードに対応する VM における処理は vm.c の mrb_vm_exec() を読むこと

特殊
====

* OP_NOP

  - なにもしない

ロード系
========

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

変数操作系
==========

よー分からんし後回しにしとく


制御系
======

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

mrb_irep (instruction representation の略？)
**********************************************

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

...

mruby 実行形式
**************

* mruby コマンドってやつ。コンパイル前のスクリプト .rb 、もしくはコンパイル済バイトコード .mrb を実行する

  - mrbgems/mruby-bin-mruby/ にコードあり

* mrb_load_irep_file_cxt() などを読んでバイトコードを読み込み、実行

  - 実行処理は mrb_top_run() を呼ぶことで行う

