mruby の値・型
##############

NOTE: Below quated codes are copied from boxing_nan.h under the MITL.

mrb_value
*********

* mruby で扱われる任意の値。
* 設定依存で boxing される

  - mrb_value のサイズをどの程度にしたいかでも変わる？ float, double, ...
  - デフォルトでは boxing_no

* boxing_no の場合は int,float,void*,sym の union になる
* mrb_vtype の型情報を持つ

mrb_int
*******

整数型。サイズは環境依存

mrb_float,mrb_double
********************

浮動小数点型

mrb_value への変換
******************

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
****************************

* mrb_str_to_cstr()

  - String クラスの mrb_value から char* へ変換

* mrb_string_to_cstr()

  - String クラスの mrb_value から const char* へ変換
  - TODO: mrb_str_to_cstr() との違いを記述

RClass
******

mruby のクラス

mrb_vtype()
***********

mrb_value の型情報(enum) を返す


mruby コアデータ構造
##########################

mrb_state
*********

* http://qiita.com/miura1729/items/822a18051e8a97244dc3 が参考になりそう。

* C で mrbgem を実装しようとするとちらほら目にする構造体
* mruby の VM の状態を保持
* 各基本クラスへのポインタやGC情報、グローバル変数などを格納する

mrb_context
************

* 実行中のメソッドのコンテキスト情報を保持しているはず

mrb_callinfo
************

* メソッド呼び出しに関する情報を保持？
* 与えられた引数の数など

RProc
******

* mruby の Proc オブジェクト型
* このオブジェクトを VM で実行して mruby で処理を行うイメージ

mruby スクリプトでも使う型
##########################

* MRB_TT_FALSE

  - 真偽値。 false を表す

* MRB_TT_FREE

  - ???

* MRB_TT_TRUE

  - 真偽値。 true を表す

...

NaN Boxing
###########

* boxing_nan で使われているテクニックについて

* mruby に限ったテクニックではない。 LuaJIT などで実現されているらしい
* http://constellation.hatenablog.com/entry/20110910/1315586703 などが参考になる

  - 恥ずかしながら double のフォーマットをこれを読んで初めて知った

NaN Boxing とは
***************

* double 型の冗長な NaN の表現を横取りして、少ないビット数で他の変数の値と型の情報を埋め込むテクニック？
* mruby の実装では 64 ビットで変数の型情報まで保持させることができる

  - とは言っても制約がある

変数表現
********

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
*********************

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
