GC
###

Overview
**********

mruby の GC はマーク＆スイープに基づくインクリメンタル GC と世代別 GC のあわせ技で実行される。
また GC 処理の間隔などを制御したり、 C ライブラリに配慮した構造がある。

Matz の mruby 本に非常にやさしい解説が書かれているので、それも合わせて読むと良さげである。

Details
**********

インクリメンタル GC
===================

* マーク＆スイープ GC をステップという単位に分割し、少しずつ処理する

  - ステップの実行単位は制御可能

* 3 つのフェーズが存在

  - ルートスキャンフェーズ

    * GC のルートオブジェクトを検出する

  - マークフェーズ

    * 生きてるっぽいオブジェクトをマークする

  - スイープフェーズ
    
    * マークされていないオブジェクトを死んでいるものとみなし回収する

世代別 GC
=========

* 作られてからの経過時間によって「世代」を分離し、若い世代のオブジェクトを頻繁に GC する
* mruby では 2 世代で GC を行う

  - マイナー GC

    * 若い世代のみ対象にした GC

  - メジャー GC

    * 若い世代のみならず、古い世代も対象にした GC

GC の実行タイミング
====================

* インクリメンタル GC

  - オブジェクトに新しくヒープを割り当てる際に、生きているオブジェクトがしきい値を超えていた場合に実行する
  - しきい値には後述する interval_ratio という係数を付けることが可能

* フル GC

  - 手動で実行するか、 mrb_realloc_simple() で割り当てに失敗した場合のみ実行する

GC 周りの基本構造
==================

* GC root

  - GC のマーク&スイープ操作の起点となるノード
  - root から辿れるノードにはマークが付けられる
  - GC_ROOT_NAME でマクロ定義されているシンボル名で、グローバル変数の Array としてルート情報が保持される

* GC color

  - オブジェクトのマーク状況に関する情報
  - 三色抽象化 (tricolour abstraction) に基づく情報のはず
  - white
  
    * マークされていない状態。 GC の対象となる。
    * 実際には white-A, white-B という 2 状態が存在するので
    * white-A

      - 現在の GC サイクル中に割り当てられた、新しいオブジェクト

    * white-B

      - スイープのターゲットとして確定したオブジェクト
  
  - gray
  
    * 処理途中であることを示す状態
    * マークされているが、その子オブジェクトはマークされていない状態
  
  - black
  
    * 子オブジェクトも含めてマークされている状態

* arena

  - C のコードで生成したオブジェクトを管理する領域
  - gc_protect() で渡されたオブジェクトが arena で管理される
  - arena_idx より低い領域で管理されるオブジェクトは、スキャンフェーズで生きているものとみなされる
  - mrb_gc_arena_save() / mrb_gc_arena_restore() で arena_idx を操作し、 GC 対象範囲を変更することも可能

::

  +-+-+-+-+-+-+-+-+-+-+-+-+-+- ~ -+
  | | | | | | | | | | | | | |     |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+- ~ -+
  ^ <-black-> ^ <-----white-----> ^
  0        arena_idx          arena_capa
     
* mrb_gc

  - sweeps

    * マークフェーズの結果、スイープのターゲットとなった heap_page のリスト

  - live

    * 生きているオブジェクトの数
    * この数がしきい値を超えた場合、 GC を実行する
  
  - arena

    * arena 領域

  - arena_capa

    * arena の容量

  - arena_idx

    * arena のインデックス

  - state

    * GC が今どのフェーズにいるのかを示す状態変数

  - gray_list

    * マークフェーズの結果の、 gray マークされたオブジェクトのリスト

  - atomic_gray_list

    * マークフェーズの最後に、再度 gray マークされることを保証すべきオブジェクトのリスト
    * ライトバリア関数の使い方によってはこのリストが使われる

  - live_after_mark

    * マークフェーズの最後に、生きているとみなされる(black な)オブジェクトの数

  - threshold

    * GC 実行タイミングを図るためのしきい値

  - interval_ratio

    * threshold の計算に用いる係数
    * live が threshold を超えたときにインクリメンタル GC が実行される
    * この値が低い（100 に近い）ほど、頻繁に GC が実行されることになる

  - step_ratio

    * インクリメンタル GC の 1 ステップでどのくらい処理を進めるかの計算に用いる係数

  - disabled

    * GC を無効化しているかどうかのフラグ

  - full

    * フル GC を実行するかどうかのフラグ

  - generational

    * 世代別 GC を実行するかどうかのフラグ

  - out_of_memory
  - majorgc_old_threshold;

* mrb_heap_page

  - prev
  - next

    - ヒープのリストの、前のノードと後ろのノードへのポインタ

  - free_prev
  - free_next

    - ヒープのフリーなリストの、前のノードと後ろのノードへのポインタ

  - old
  
    * そのページの世代を bool で示す
    * true なら古い世代。 false なら若い世代(マイナー GC の対象)

Internals
==========

GC のエントリポイント
---------------------

* mrb_obj_alloc()

  - オブジェクトのためのメモリを割り当てる
  - 割り当て前に、現在生き残っているオブジェクトがしきい値を超えている場合は GC を動作させる
  - free_heaps からオブジェクト用に freelist を割り付ける

    * もし free_heaps が無い場合は add_heap() を読んで継ぎ足す

  - paint_partial_white() を呼んでおく

* incremental_gc_step()

  - インクリメンタル GC をステップ実行する
  - step_ratio 係数をかけた

* incremental_gc_until()

  - インクリメンタル GC を、指定した状態に遷移するまで実行する

* incremental_gc()

  - インクリメンタル GC 処理の本体
  - ROOT 状態の場合:

    * root をスキャンする
    * 次の状態を MARK にする

  - MARK 状態の場合:
  - SWEEP 状態の場合:

スキャンフェーズ関連
--------------------

* root_scan_phase()

  - ROOT フェーズの処理
  - sweep されるべきでない obj を片っ端から mark していく。これらが mark の root になる

    * irena_idx 以下の arena に格納されている obj
    * Object クラス
    * ... などなど

マークフェーズ関連
------------------

* incremental_marking_phase()

  - MARK フェーズの処理

* add_gray_list()

  - 引数の obj を gray に着色し、 gray_list に追加する

* gc_gray_mark()

  - gc_mark_children() を呼び出してマークする
  - 個要素の数を取得して戻り値として返す

* gc_mark_children()

  - 引数の obj を black でマークする
  - obj の型によっては、例えばクラスやオブジェクトだったらインスタンス変数を、 gray でマークする

* gc_mark_gray_list()

  - gray_list を順番になめて、 gray になってなかったら gray にする

* final_marking_phase()

  - マークフェーズの後処理を行う
  - atomic_gray_list に格納された obj を再度マークする

* prepare_incremental_sweep()

  - スイープフェーズの前処理を行う

    * フェーズ状態の変更やマーク・スイープに使用するリストのセット

* clear_all_old()

  - 世代別 GC を一旦無効化した上で GC サイクルを次のスキャンフェーズまで実行する
  - こうすることで古い世代を若い世代として処理し直すことが可能になる

スイープフェーズ関連
----------------------

* incremental_sweep_phase()

  - sweeps リストを順番になめていく
  - 各 obj を obj_free() に突っ込んでゆき、 freelist に加えていく

アロケーション周り
---------------------

* mrb_realloc_simple()

  - アロケーション処理の根の部分
  - mrb->allocf に割り当てを行ってもらう
  - 割り当てに失敗した場合にフル GC を実行して再挑戦する

* mrb_realloc()

  - mrb_realloc_simple を呼んでメモリを割り当てる
  - 十分なメモリを割り当てられなかった場合は out_of_memory フラグを true にして例外を投げる

* mrb_malloc()

  - mrb_realloc() を、割り当て済みポインタを nullptr にしているだけのラッパー

* mrb_malloc_simple()

  - mrb_realloc_simple() を、割り当て済みポインタを nullptr にしているだけのラッパー

* mrb_calloc()

  - nelem * len のサイズの領域を mrb_malloc() で割り当てる
  - 確保した領域はゼロクリアされる

* mrb_free()

  - mrb->allocf を第三引数 0 で呼び出して free してもらう

* mrb_object_dead_p()

  - 引数で渡されたオブジェクトが死んでいる扱いであれば true を返す
  - 処理的には white でマークされていないか、型情報が FREE でないかをチェックしている

ヒープ管理周り
--------------

* link_heap_page()

  - 引数で渡された heap_page を、 heaps のリストに加える

* unlink_heap_page()

  - 引数で渡された heap_page を、 heaps のリストから外す

* link_free_heap_page()

  - 引数で渡された heap_page を、 free_heaps のリストに加える

* unlink_free_heap_page()

  - 引数で渡された heap_page を、 free_heaps のリストから外す

* add_heap()

  - 新しい heap_page を mrb_calloc で割り当てる
  - 割り当てが成功したなら、 heaps, free_heaps リストにそれを加える

* free_heap()

  - heaps リストの各ページをチェックしていく
  - obj_free() でデストラクタを呼び出していく
  - mrb_free() で指定の heap_page を解放する

APIs
*******

C APIs
======

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

  - フル GC を実行する
  - 古い世代も GC 対象に戻す

* mrb_incremental_gc()

  - インクリメンタル GC を実行する

* mrb_garbage_collect()

  - mrb_full_gc() のエイリアス

* mrb_gc_mark()

  - 引数に渡された RBasic 型の変数をマークする

    * 実際の処理としては、 obj を gray_list に追加している

  - mrb_value を渡せる mrb_gc_mark_value() マクロも存在する

* mrb_gc_protect()

  - arena のキャパを増やし、引数の mrb_value を arena の idx の領域に格納する
  - 指定の mrb_value を arena 内に格納して、（直近の GC から？）保護するイメージ

* mrb_field_write_barrier(mrb_state \*mrb, struct RBasic \*obj, struct RBasic \*value)
  
  - black な obj が参照する white な value に対してライトバリアを張る
  - gray_list に value を追加する

* mrb_write_barrier(mrb_state \*mrb, struct RBasic \*obj)

  - obj にライトバリアをはる
  - obj を gray にマークした上で、 atomic_gray_list に追加する

mruby APIs
==========

mruby アプリケーションから GC の振る舞いに干渉するための Module 、 "GC" が提供されている

* GC#start

  - Full GC を実行する

* GC#enable

  - GC を有効にする

* GC#disable

  - GC を無効にする

* GC#interval_ratio

  - interval_ratio を読み出す

* GC#interval_ratio=

  - interval_ratio を更新する

* GC#step_ratio

  - step_ratio を読み出す

* GC#step_ratio=

  - step_ratio を更新する

* GC#generational_mode

  - 世代別 GC モードかどうかを返す

* GC#generational_mode=

  - 世代別 GC モードにするかどうかを設定する

