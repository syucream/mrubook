メモリ管理
###########

Overview
*********

mruby の内部的なページベースのメモリ管理方法について記述する

Details
*******

mrb_pool 構造体
===============

* mrb_state へのポインタと、 mrb_pool_page の先頭ノードへのポインタを持つ

.. code :: c

  struct mrb_pool {
    mrb_state \*mrb;
    struct mrb_pool_page \*pages;
  };

mrb_pool_page 構造体
=====================

* 各ページノードの要素と次のノードと末尾ノードの使用済み領域の終端へのポインタをもつ

.. code :: c

  struct mrb_pool_page {
    struct mrb_pool_page \*next;
    size_t offset;
    size_t len;
    void \*last;
    char page[];
  };

APIs
****

struct mrb_pool\* mrb_pool_open(mrb_state\*)
=============================================

* mrb_pool を初期化する

void mrb_pool_close(struct mrb_pool\*)
=======================================

* mrb_pool とそれに属するページのリソースを開放する

void\* mrb_pool_alloc(struct mrb_pool\*, size_t)
=================================================

* mrb_pool に size_t サイズだけ空きのあるページを用意する

  - もし既存のページに size_t 分の空きがあれば last を更新する
  - もし既存のページに size_t 分の空きがなければ、新たに領域を確保する

