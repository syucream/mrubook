Fiber
#####

Overview
********

* mruby の軽量スレッドである Fiber について、 mruby-fiber/ を読む

Details
*******

Fiber クラスについて
=====================

* mruby 組み込み型のひとつ
* 実装は割とシンプル

  - もともと、 mrb_state, mrb_context が「前のコンテキストに戻る」ことを考慮した作りになっている
  - Fiber は mrb_context を持つ。これを mrb_state に対して「次/前のコンテキストに移る」操作を提供することで resume/yield を実現する

C の RFiber の定義
==================

* 単なる RiteVM の実行コンテキストを持ってるだけのオブジェクト

.. code :: c

  struct RFiber {
    MRB_OBJECT_HEADER;
    struct mrb_context *cxt;
  };

fiber_switch()
===============

* まずは現在の状態をチェック。下記条件にマッチした場合は FIBER_ERROR を raise する

  - transfer された後の fiber を resume した
  - 実行中の fiber を resume した
  - 既に終了した fiber を resume した

* value へ代入しているあたりはよくわからん
* mrb_vm_exec() を介して RiteVM に mrb_context の処理の実行を開始させる

  - yield されたら返ってくる。その時の戻り値を取得する
  - mrb_vm_exec() 呼び出し前に退避させておいた、 resume 元の mrb_context を mrb_state に戻す

* 最後に戻り値を返して終了

mrb_fiber_yield()
==================

* yield しようとしている fiber が root になっていないかチェック

  - root からは yield 先がいない！ FIBER_ERROR を raise する

* mrb_state の mrb_context を prev のもの、つまり親ファイバのものに戻す
* fiber_result() で yield クラスメソッドの引数をそのまま親に返す

fiber_result()
===============

渡された array? を元に、 nil か一つの値か、あるいは array 全体を取り出す

APIs
*******

initialize
==========

* MRB_ARGS_NONE であるが実際は block を引数にとる

  - block が取れなかったら ARGUMENT_ERROR を raise する

* RiteVM のコンテキストを初期化する

  - スタック領域の割当と初期化
  - receiver （ってなんだろ）を block からコピー
  - callinfo の割当と初期化
  - callinfo 周りの調整
  
    * block のポインタをコピーしたり pc を設定したりレジスタ数を設定したり
    * ここで設定した callinfo を元に resume されると処理実行が行われるのか

* 最後に fiber の状態を初期化

  - fiber は状態を持つ（そりゃそうか）
  - コンストラクタが終わった状態では MRB_FIBER_CREATED

resume
=======

* 任意個の引数を受ける
* 実際の実行は fiber_switch() で。。。

transfer
========

* TODO: 気が向いたら読む

alive?
======

* TODO: 気が向いたら読む

\==
====

* TODO: 気が向いたら読む

クラスメソッド
==============

yield
-------

* 任意個の引数を受ける
* 実際の実行は mrb_fiber_yield() で。。。

current
-------

* TODO: 気が向いたら読む

状態
=======

* mrb_context メンバの status で管理される(Fiber以外でもこの変数使ってるのかな・・・？)
* 以下のような状態をもつ

.. code :: c

  enum mrb_fiber_state {
    MRB_FIBER_CREATED = 0,
    MRB_FIBER_RUNNING,
    MRB_FIBER_RESUMED,
    MRB_FIBER_SUSPENDED,
    MRB_FIBER_TRANSFERRED,
    MRB_FIBER_TERMINATED,
  };

状態遷移


* TODO: 詳細確認する

