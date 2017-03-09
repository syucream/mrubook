= CRuby との比較

mruby の CRuby との差異は RiteVM にとどまりません。
ここでは主に、 mruby で使用されるハッシュ関数ライブラリ khash と ガベージコレクションについて簡単に触れてみます。

== khash

=== khash の基礎

khash は Hash の実装などで使用される mruby のハッシュテーブルのデータ構造と操作関数のセットです。
khash の大きな特徴として、実装がヘッダで完結し、ほぼすべてマクロで書かれていることが挙げられます。
ちなみに khash はゼロから実装されたわけではなく、 @<href>{https://github.com/attractivechaos/klib, klib} を参考に作られています。
また、 CRuby においては st_table というまた別のハッシュテーブルを用いています。
この点については、やはり 「Rubyソースコード完全解説」@<bib>{rhg} を参照してみていただければ幸いです。

khash によるハッシュテーブルは下記のようなデータ構造を持ちます。
ちなみにハッシュテーブルの構造体はマクロで任意の文字列が末尾に付与されて命名されます。

//table[kh][kh ハッシュテーブルの構造]{
名前	内容 
-------------------------------------------------------------
n_buckets	メモリを確保した最大要素数
size	現在使われている（削除済みでない）要素数
n_occupied	一度でも値がセットされたことのある要素数
keys	キー配列
vals	値配列
ed_flags	各要素が空や削除済みかどうかを管理するフラグ配列。(empty delete flags?)
//}

khash ですが、主に下記のような操作をサポートしています。
kh_init で初期化、 kh_begin と kh_end でイテレーション、 kh_get と kh_put でキーによる操作、 kh_keys と kh_val で要素の参照、 kh_destroy で終了処理というのが基本的な操作かと思われます。

//table[khash][khash の操作関数]{
名前	内容 
-------------------------------------------------------------
kh_init_size	サイズを明示してハッシュテーブルを初期化する
kh_init	サイズを明示せず、デフォルト値（32）でハッシュテーブルを初期化する
kh_destroy	ハッシュテーブルを破棄してメモリを解放する
kh_clear	全要素をクリアする（空フラグを立てる）
kh_resize	指定の領域が格納できるようメモリを確保し直す。元の要素は引き継ぐ
kh_put	ハッシュテーブルにキーをセットする
kh_put2	ハッシュテーブルにキーをセットする。 kh_put と異なり第 5 引数に既にキーが存在したか、削除済み領域を再利用するか、それとも空の領域にセットするかの結果を受け取れる
kh_get	指定のキーに対応するハッシュテーブル中の要素のインデックスを返す
kh_del	指定のインデックスに対応するハッシュテーブル中の要素の削除フラグを立てる
kh_exist	指定のインデックスに対応する要素が空でも削除でもない場合 true を返す
kh_key	指定のインデックスに対応する keys の要素を返す
kh_value	kh_val のエイリアス
kh_begin	ハッシュテーブルの先頭要素のインデックスを返す
kh_end	ハッシュテーブルの末尾要素のインデックスを返す
//}

これらのデータ構造と操作関数を使用するには宣言、定義を行うためのマクロ KHASH_DECLARE と KHASH_DEFINE を呼び出しておく必要があります。

なお、 khash ではハッシュ関数は KHASH_DEFINE を呼び出す際に自分で指定することになります。
khash のヘッダで kh_int_hash_func() と kh_int64_hash_func() 、 kh_str_hash_func() の三種類が定義されるので、これを用いても良いです。
またハッシュ値の衝突を検出するための比較関数も KHASH_DEFINE を呼び出す際に指定する必要があります。
比較関数は kh_get と kh_put で用いられ、もし衝突を見つけた際はインデックスをインクリメントしていき空き要素を線形探索します。

=== Hash への適用

折角なので mruby の Hash の実装にどのように khash が用いられているかも見てみましょう。
ソースコードは src/hash.c となります。

まず Hash における khash の宣言と定義ですが、キーと値の型はそれぞれ mrb_value と mrb_hash_value となります。
ハッシュ関数と比較関数も mrb_hash_ht_hash_func() と mrb_hash_ht_hash_equal() という Hash の実装専用の関数を指定しています。
これらの関数には、キーの mrb_value 型変数を型ごとにハッシュ値計算、比較するようなロジックが含まれています。

mruby の Hash のメソッドは khash のハッシュテーブルにオブジェクトヘッダや管理情報用テーブルを含めた RHash 構造体への操作として実装されています。
例えば initialize は大枠で言えば kh_init を呼び出す形でハッシュテーブルの初期化を実現しています。
また [] では kh_get() によるキーの探索を行い kh_value() で値を返却、 []= では kh_put2() でキーをセットした後 kh_value() で値を代入します。


== ガベージコレクション

「mruby のすべて」@<bib>{mruby_book} という書籍では全 5 章のうち 1 章まるまるガベージコレクション（以下、 GC）の話題に割いています。
その事実からも mruby の GC が重要な話題であることが分かり、 CRuby と比較する上でもぜひ触れておきたい話を言えるでしょう。
ただし本書では GC の戦略や設計の詳細までは解説しません。
その点は既に存在するその書籍を読んで補完していただければ幸いです。

=== mruby の GC の基礎

mruby ではいわゆるマークスイープ方式の GC を行っています。
ただし単純に GC するのではなく、オブジェクトの世代を若い世代と長命な世代に分けて若い世代だけ頻繁に GC することでオーバーヘッドを減らす世代別 GC を採用しています。
世代別 GC では頻繁に実行される、若い世代のみ対象にするマイナー GC 、全世代を対象にするメジャー GC の二種類に範囲を分離されて実行されます。

さらに mruby の利用想定シーンの大きな部分を占める組み込み機器への適用を見越して、一回の GC サイクルで全てを行わず、少しずつ GC していくインクリメンタル GC という手法も採用しています。
mruby のインクリメンタル GC ではマークスイープを 3 つのフェーズ、 GC のルートオブジェクトを検出するルートスキャンフェーズ、生きていると思われるオブジェクトをマークするマークフェーズ、マークされていないオブジェクトを死んでいるとみなし回収するスイープフェーズに分割して実行します。
ちなみにこのインクリメンタル GC の採用のため、 GC の途中でオブジェクトが書き換わる可能性が発生し、そのハンドリングのためにライトバリアという機構を導入したりしています。

=== GC に関するオブジェクトの状態

mruby では各オブジェクトに「色」の情報を持たせ、色が何になっているかによってオブジェクトの生死や世代を管理します。
この色情報ですが、白、灰色、黒という三色で管理されます。

これは「ガベージコレクション本」@<bib>{gc_book} に記載されている三色抽象化 (tricolour abstraction) に近いものがあります。
ただし mruby はインクリメンタル GC をするので GC のサイクルの途中に新たなオブジェクトが生成され、すぐに回収して良いか判断がつかない場合があります。
この問題のため、 mruby では白色についてさらに 2 つの状態を用意しています。

これらの状態とそれが指すオブジェクトの状態は下記の通りになります。
ちなみに mruby の実装としては、これらの色情報は 3 ビットで表現されます。

//table[gc_color][GC の色情報]{
色種別	内容 
-------------------------------------------------------------
White-A	GC のサイクルの途中に生成されたオブジェクト。その GC サイクルではスイープの対象にならない
White-B	死んでいるとみなされるオブジェクト。その GC サイクルでスイープの対象になる
Gray	生きているとみなされるオブジェクト。ただしそのオブジェクトの子オブジェクトは生きているか分からない。世代別 GC においてマイナー GC の対象になる
Black	生きているとみなされるオブジェクト。そのオブジェクトの子オブジェクトを含めて生きている。世代別 GC においてマイナー GC の対象にならない
//}

=== アリーナ

mruby の GC において他にややこしい点として、しばしば C で mrbgem が実装されることが挙げられます。
なにがややこしいかというと、 C の関数を実行中に生成された mruby のオブジェクトを回収対象にして良いのか判断がつかないのです。
mruby ではこの問題への対応のため、アリーナという概念を導入しています。

アリーナは GC においてルートオブジェクトとして扱うオブジェクトを羅列した配列です。
C 関数から生成したオブジェクトは自動的にこのアリーナに含められ、関数実行中はルートオブジェクトとして扱われる（ルートスキャンフェーズで Black に塗られ、回収されることがない）ようになります。
また C 関数から明示的にアリーナによる GC の対象を制御する際は、インデックス操作を行うことでルートオブジェクトから外れるようにできたりします。
このテクニックは特に C で mrbgem を実装して、その中でループでオブジェクトを生成しまくる時などに気をつける必要のある点だったりします。

//cmd{
+-+-+-+-+-+-+-+-+-+-+-+-+-+- ~ -+
| | | | | | | | | | | | | |     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+- ~ -+
^ <-black-> ^ <-----white?----> ^
0        arena_idx          arena_capa
//}

=== GC の実装

mruby の GC の実装は src/gc.c にあります。

==== GC の実行タイミング

インクリメンタル GC はオブジェクトに新しくヒープを割り当てる際に、生きているオブジェクトがしきい値を超えていた場合に実行します。
このしきい値には後述する interval_ratio という係数を付けることが可能です。

フル GC は手動で実行するか、 mrb_realloc_simple() で割り当てに失敗した場合のみ実行します。

==== GC 周りの基本データ構造

GC に関する情報を表現する mrb_gc のデータ構造は下記のようになっています。

//table[mrb_gc][mrb_gc の構造]{
名前	内容 
-------------------------------------------------------------
heaps	TODO
sweeps	マークフェーズの結果、スイープのターゲットとなった heap_page のリスト
free_heaps	TODO
live	生きているオブジェクトの数。この数がしきい値を超えた場合、 GC を実行
arena	アリーナ領域
arena_capa	アリーナの容量
arena_idx	アリーナのインデックス
state	GC が今どのフェーズにいるのかを示す状態変数
gray_list	マークフェーズの結果の、 gray マークされたオブジェクトのリスト
atomic_gray_list	マークフェーズの最後に、再度 gray マークされることを保証すべきオブジェクトのリスト
live_after_mark	マークフェーズの最後に、生きているとみなされる(black な)オブジェクトの数
threshold	GC 実行タイミングを図るためのしきい値
interval_ratio	threshold の計算に用いる係数。 live が threshold を超えたときにインクリメンタル GC が実行される
step_ratio	インクリメンタル GC の 1 ステップでどのくらい処理を進めるかの計算に用いる係数
full	フル GC を実行するかどうかのフラグ
generational	世代別 GC を実行するかどうかのフラグ
majorgc_old_threshold	TODO
//}

また GC のマークスイープ操作の起点となるルートノードは GC_ROOT_NAME でマクロ定義されているシンボル名で、グローバル変数の Array としてルート情報が保持されます。

アリーナについてですが、 mrb_gc の arena がアリーナ配列、 arena_idx がルートオブジェクトとして扱うべきインデックスになります。
ルートスキャンフェーズにおいて arena_idx より低い番地に格納されるオブジェクトは、スキャンフェーズで生きているものとみなされます。
arena_idx は C の関数から操作可能でして、 mrb_gc_arena_save() でその時の arena_idx の取得、 mrb_gc_arena_restore() で arena_idx の再設定が可能です。
また、 gc_protect() を呼び出すことでオブジェクトをアリーナで管理するように操作することもできます。

