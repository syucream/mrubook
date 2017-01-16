khash
########

Overview
************

* mruby の実装でひろく利用されるハッシュの実装です。 [attractivechaos/klib](https://github.com/attractivechaos/klib) を参考に実装されているようです。
* ほぼ全てが khash.h でマクロで実装されています
* CRuby の st_table の代替とみることができるはず？


Details
*********


APIs
*****


KHASH_DECLARE(name, khkey_t, khval_t, kh_is_map)
=====================================================

* khash を利用した新しいハッシュテーブルの型を宣言します

KHASH_DEFINE(name, khkey_t, khval_t, kh_is_map, __hash_func, __hash_equal)
============================================================================

* khash を利用した新しいハッシュテーブルの型を定義します

kh_get(name, mrb, h, k)
=========================

* khash インスタンスのキーを取得します

kh_put(name, mrb, h, k)
=========================

* khash インスタンスに新しいキーを追加します

kh_val(h, x)
=========================

* khash インスタンスの値を参照します

kh_value(h, x)
=========================

* khash インスタンスの値を参照します
* kh_val() のエイリアスです
