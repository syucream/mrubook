GC
###

GC 周りの基本
**************

* GC root

  - GC のマーク&スイープ操作の起点となるノード
  - root から辿れるノードにはマークが付けられる
  - GC_ROOT_NAME でマクロ定義されているシンボル名で、グローバル変数の Array としてルート情報が保持される

* GC color

  - white, black, gray の三色がある
  - ライトバリア関係の情報？

APIs
*****

* mrb_gc_arena_save() / mrb_gc_arena_restore()

  - Matz にっきを読もう: http://www.rubyist.net/~matz/20130731.html
  - 簡単にまとめると下記のようなかんじ？

    1. C 実装内でオブジェクト生成したら GC 対象外になる
    2. ただし mrb_gc_arena_save() から mrb_gc_arena_restore() までに生成したオブジェクトは GC 対象になる

* mrb_gc_register() / mrb_gc_unregister() 周り

  - 引数の mrb_value を GC root に登録/削除することで、 GC の対象外として登録/解除を行う
  - mrb_gc_unregister(mrb, obj)

    * GC root 変数配列から、引数に渡された obj と同値のものを見つけて、配列から削除する
    * 削除はその値以外を memmove することで実現
 
* mrb_full_gc()

  - GC を C のコードからフルで実行する
  - TODO: ちゃんと src/gc.c を読む

* mrb_incremental_gc()

  - インクリメンタル GC を C のコードから実行する
  - TODO: ちゃんと src/gc.c を読む
  
    * 具体的な仕事は incremental_gc_until() や incremental_gc_step() がやってそう

* mrb_garbage_collect()

  - mrb_full_gc() のエイリアス

* mrb_gc_mark()

  - 引数に渡された RBasic 型の変数をマークする(使用中の変数になる)
  - mrb_value を渡せる mrb_gc_mark_value() マクロも存在する

* mrb_gc_protect()

  - TODO: よく分からない。ちゃんと src/gc.c を読む

* mrb_write_barrier(mrb_state \*mrb, struct RBasic \*obj)

  - obj にライトバリアをはる
  - 実際には gray リストに obj を突っ込む


