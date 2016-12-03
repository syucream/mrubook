Object
########

Overview
**********

オブジェクトについては基本的に RBasic(基本構造), RObject(クラスを実体化したもの。インスタンス変数をもつ)、 RFiber(ファイバー。コンテキスト情報をもつ) の 3 種が存在する

Details
*******

共通のデータ構造
================

.. code :: c

  #define MRB_OBJECT_HEADER \
    enum mrb_vtype tt:8;\
    uint32_t color:3;\
    uint32_t flags:21;\
    struct RClass *c;\
    struct RBasic *gcnext

* tt

  - 型情報

* color

  - GC に関する情報・・・のはず

* flags

  - TODO: あとで詳細を調べる

* c

  - クラスの詳細な定義情報へのポインタ

* gcnext

  - GC に関する情報・・・のはず

RBasic
*******

* 共通のデータ構造のみメンバにもつ

.. code :: c

  struct RBasic {
    MRB_OBJECT_HEADER;
  };
  
RObject
=======

* 追加でインスタンス変数テーブルをメンバにもつ

.. code :: c

  struct RObject {
    MRB_OBJECT_HEADER;
    struct iv_tbl *iv;
  };
