便利関数などなど
########################################

* だいたい `include/` 以下を読んでおけば OK です。

定義する系
**********

クラス・モジュール関係
======================

だいたい名前の通りのしごとをします

* mrb_define_class()
* mrb_define_module()
* mrb_singleton_class()
* mrb_include_module()
* mrb_prepend_module()
* mrb_define_method()
* mrb_define_class_method()
* mrb_define_singleton_method()
* mrb_define_module_function()

定義を削除することもできます

* mrb_undef_method()
* mrb_undef_class_method()

定義の構造も柔軟に設定可能です

* mrb_define_class_under()
* mrb_define_module_under()

定数・変数系関係
========================

* mrb_define_const()

メソッドの引数定義
========================

便利マクロがいくつか。 `mrb_define_method()` などの第三引数の指定などに使います。

* MRB_ARGS_REQ(n)

  - n の数だけ引数を必須とします
  
* MRB_ARGS_OPT(n)

  - n の数だけ引数を任意とします
  
* MRB_ARGS_ARG(n1,n2)

  - n1 の数だけ引数を必須, n2 の数だけ引数を任意とします
  
* MRB_ARGS_REST()
* MRB_ARGS_POST(n)
* MRB_ARGS_KEY(n1,n2)
* MRB_ARGS_BLOCK()

  - 引数として block を受け取ります
  
* MRB_ARGS_ANY()

  - 任意個の引数をとります
  
* MRB_ARGS_NONE()

  - 引数なしです
  
メソッドの引数取得
========================

* mrb_get_args() でメソッドに渡った引数の取得を行います
* format 文字列で引数の型や数などを取得できます
* 想定と異なる数の引数が渡されたなどの場合に例外を投げます

リソース管理系
**************

new する系
===========

* mrb_obj_new()

  - 新しいオブジェクトを new します
  - mruby の世界でいう ExampleClass.new 的な操作になります

* mrb_class_new_instance()

  - mrb_obj_new() のエイリアスです

* mrb_instance_new()

  - mrb_obj_new() とほぼおなじ？
  - 引数にブロックを受け取れるとか引数の型が違うとかに差分がありそう

 
エラー・例外系
**************

* mrb_raise

  - 例外を投げます
  - mrb_state の jmp 先がなかったら abort() します
  - mrb_raisef() を読んだ場合は例外メッセージをフォーマット指定できます

未分別
**************

* mrb_gv_get(mrb, sym)

  - sym が指すグローバル変数を取得する
  - やっていることはインスタンス変数の取得とほぼおなじ
    
    * globals グローバル変数テーブル

* mrb_gv_set(mrb_state \*mrb, mrb_sym sym, mrb_value v)

  - v を新たにグローバル変数として、 sym シンボルで登録する

* mrb_obj_eq(mrb, v1, v2)

  - v1 と v2 を比較し、同値の場合に true を返す
  - 比較方法

    * v1 と v2 の型を比較
    * 型ごとにキャストした結果を比較

* mrb_obj_equal()

  - mrb_obj_eq() のエイリアス

