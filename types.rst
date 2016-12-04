値と型
#######

Details
*******

mrb_value
=========

* mruby で扱われる任意の値。
* 設定依存で Boxing される

  - デフォルトでは boxing_no
  - NaN Boxing や Word Boxing のオプションが提供される。使用する場合はそれらの制約に気をつける必要がある

RClass
======

* mruby のクラス
* iv は Instance variable table 変数

  - セグメント(TODO: 定義をちゃんと調べる)ごとのサイズとルートセグメントへのポインタをもつ　

* mt

  - struct kh_mt はそのままの名前で定義されていないので注意！
  
    * kh_xxx は khash.h の KHASH_DECLARE マクロでプリプロセッサではじめて定義される
  
  - mt は mark table ? GC に使うっぽい？ TODO: gc.c とか読む
  - khash は mruby の hash table のはず TODO: khash の詳細を追う

* super

  - そのまんま、親クラスのポインタ

.. code :: c

  struct RClass {
    MRB_OBJECT_HEADER;
    struct iv_tbl \*iv;
    struct kh_mt \*mt;
    struct RClass \*super;
  };

RObject
========

* mruby のオブジェクト
* mrb_obj_ptr マクロで mrb_value から RObject へ変換できる

* 下記のような定義になるはず

  - 一部メンバは MRB_OBJECT_HEADER マクロで展開される

.. code :: c

  struct RObject {
    // MRB_OBJECT_HEADER
    enum mrb_vtype tt:8
    uint32_t color:3;
    uint32_t flags:21;
    struct RClass \*c;
    struct RBasic \*gcnext;

    // RClass のみ
    struct iv_tbl \*iv;
  };

* struct RClass*

  - オブジェクトのクラスへのポインタ
  - mrb_class() 関数では、引数の mrb_value が組み込みクラス以外であった場合にはこの値を返す

mrb_int
=======

* 整数型。サイズは環境依存

mrb_float
====================

* 浮動小数点数型
* MRB_USE_FLOAT を define しない限りは実体は C の double 型になる


mrb_value の型変換
==================

別の型から mrb_value への変換
-----------------------------

mrb_int などから mrb_value 型表現へ変換する関数が存在

* mrb_float_value()
* mrb_fixnum_value()
* mrb_symbol_value()
* mrb_obj_value()
* mrb_nil_value()
* mrb_false_value()
* mrb_true_value()
* mrb_bool_value()
* mrb_undef_value()

mrb_value から別の型への変換
-----------------------------

* mrb_str_to_cstr()

  - char* へ変換

* mrb_string_to_cstr()

  - const char* へ変換
  - TODO: mrb_str_to_cstr() との違いを記述

* mrb_class_ptr(v)

  - struct RClass* へ変換

* mrb_obj_ptr() マクロ

  - RObject* へ変換

* mrb_ary_ptr(v) / RARRAY(v)

  - struct RArray* へ変換
  
* mrb_ptr() マクロ

  - void* へ変換
  - Boxing は内部的に勝手に考慮する

* mrb_cptr() マクロ

  - mrb_ptr() マクロのエイリアス

mrb_value の型情報の確認
========================

mrb_vtype()
-----------------------------

* mrb_value の型情報(enum mrb_vtype) を返す

mrb_value の型チェック
-----------------------------

* mruby のヘッダでいくつか型チェック用のマクロが用意されている

  - NaN Boxing されているかどうかも考慮してチェックする

* mrb_fixnum_p(o)

  - o が Fixnum 型であれば true を返す

* mrb_undef_p(o)

  - o が Undef 型であれば true を返す

* mrb_nil_p(o)

  - o が Nil 型であれば true を返す

* mrb_bool(o)

  - o が True 型であれば true を返し、 False 型であれば false を返す

* mrb_float_p(o)

  - o が Float 型であれば true を返す

* mrb_symbol_p(o)

  - o が Symbol 型であれば true を返す

* mrb_array_p(o)

  - o が Array 型であれば true を返す

* mrb_string_p(o)

  - o が String 型であれば true を返す

* mrb_hash_p(o)

  - o が Hash 型であれば true を返す

* mrb_cptr_p(o)

  - o が Cptr 型であれば true を返す

* mrb_exception_p(o)

  - o が Exception 型であれば true を返す

* mrb_test(o)

  - mrb_bool(o) のエイリアス

* mrb_regexp_p

  - 第二引数の mrb_value が Regexp 型であれば true を返す
  - これはマクロではなく C の関数

mruby スクリプトでも使う型
==========================

* 合計 23 個の型が存在する
* MRB_TT_FALSE

  - 真偽値。 false を表す

* MRB_TT_FREE

  - ???

* MRB_TT_TRUE

  - 真偽値。 true を表す

* MRB_TT_FIXNUM

  - 整数型

...

* MRB_TT_OBJECT

  - オブジェクト型
  - TODO: 詳細は別途調べる

* MRB_TT_CLASS

  - クラス型
  - TODO: 詳細は別途調べる

...

NaN Boxing
===========

* boxing_nan で使われているテクニックについて

* mruby に限ったテクニックではない。 LuaJIT などで実現されているらしい
* http://constellation.hatenablog.com/entry/20110910/1315586703 などが参考になる

  - 恥ずかしながら double のフォーマットをこれを読んで初めて知った

NaN Boxing とは
---------------

* 前提として、 double 型は NaN の表現に使用しているビット幅が冗長
* 一部分を横取りして、少ないビット数で他の変数の値と型の情報を埋め込むテクニックが Nan Boxing
* mruby の実装では 64 ビットで変数の型情報まで保持させることができる

  - とは言っても制約がある

変数表現
--------

64 ビットの範囲で、下記のように表現される。ちゃんと日本語化すると以下の通り

* float
  
  - double 型の NaN 領域と同じ扱いのはず？

* object
  
  - 上位 12 ビット: 1 で埋められる
  - 中位  6 ビット: オブジェクトの詳細な型の情報になる
  - 下位 46 ビット: オブジェクトの実際の値 ... のポインタ値になる

* int
  
  - 上位 16 ビット: 1111111111110001
  - 中位 16 ビット: 0000000000000000
  - 下位 32 ビット: int の実際の値

* sym(シンボル)
  
  - 上位 16 ビット: 1111111111110001
  - 中位 16 ビット: 0100000000000000
  - 下位 32 ビット: sym の実際の値

::

  float : FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF
  object: 111111111111TTTT TTPPPPPPPPPPPPPP PPPPPPPPPPPPPPPP PPPPPPPPPPPPPPPP
  int   : 1111111111110001 0000000000000000 IIIIIIIIIIIIIIII IIIIIIIIIIIIIIII
  sym   : 1111111111110001 0100000000000000 SSSSSSSSSSSSSSSS SSSSSSSSSSSSSSSS

* C の構造体では下記のように定義される

.. code :: c

  typedef struct mrb_value {
    union {
      mrb_float f;
      union {
        void *p;
        struct {
          MRB_ENDIAN_LOHI(
            uint32_t ttt;
            ,union {
              mrb_int i;
              mrb_sym sym;
            };
          )
        };
      } value;
    };
  } mrb_value;

* object の 6 ビットの型情報だけど、 mruby の型は 23 種類存在する（mrb_vtype の定義を参考）ので、これが収まるサイズにした感じか

NaN Boxing しない世界
---------------------

* つまり NaN Boxing 、あるいは Word Boxing を有効にしない場合

  - 多くのユーザはこれにあたるはず

* C の構造体では下記のように定義される

  - float, object(ポインタ), int, sym(シンボル) は一緒くたに union で宣言される
  - 型情報(tt) はそれとは別にもつ。ので環境によっては mrb_value のサイズは 64 ビット以上になる

.. code :: c

  typedef struct mrb_value {
    union {
      mrb_float f;
      void *p;
      mrb_int i;
      mrb_sym sym;
    } value;
    enum mrb_vtype tt;
  } mrb_value;
