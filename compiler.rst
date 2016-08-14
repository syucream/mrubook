mruby compiler
##############

mrbgems/mruby-compiler/, mrbgems/mruby-bin-mrbc/ あたりにあるコードを対象に記述します。

キーワードのマッチング
**********************

* `gperf <https://www.gnu.org/software/gperf/>`_ に基づいたハッシュ関数を元にキーワードのマッチングを行います

  - gperf については `このあたりの記事 <http://www.ibm.com/developerworks/jp/linux/library/l-gperf.html>`_ が参考になりそう

* keywords が入力となるキーワード定義、 lex.def がそれを gperf に食わせた結果の出力ファイルのようです。

mrb_lex_state_enum
*******************

* 構文解析器で使う状態を示すための enum 値

  - TODO: 詳細を書く

AST
***

* node_type に AST の各ノードのタイプが記載されている

パース処理
**********

mrb_parser_dump()
=================

* メインのパース処理

コード生成
**********

mrb_generate_code()
===================

* 引数に渡した parser_state を元にコード生成を行い、 RProc* を返す。
* C 実装で mruby スクリプトの文字列をパースしてコード生成する人にはお馴染みかも。

mrbc
****

* mruby スクリプトの .mrb バイトコードを生成するための実行ファイル。
* mrbgems/mruby-bin-mrbc/ にコードが存在
