Proc
####

Details
*******

RProc
=====

* あるロジックの塊を示す構造

  - mrbc によるコンパイル結果の一つとしてこれを選べる
  - RiteVM に mrb_run() を介して実行させることができる形式

.. code :: c

  struct RProc {
    MRB_OBJECT_HEADER;
    union {
      mrb_irep \*irep;
      mrb_func_t func;
    } body;
    struct RClass \*target_class;
    struct REnv \*env;
  };

APIs
****

struct RProc *mrb_proc_new(mrb_state*, mrb_irep*)
=================================================

* RProc オブジェクトを新たに生成する

  - 型は Proc になる
  - 親クラスは proc_class になる

mrb_proc_copy(struct RProc \*a, struct RProc \*b)
=================================================

  - a に b をコピーする
  - 中身としてはほぼポインタメンバのコピー。ディープコピーではない

    * irep の refcnt(参照カウンタ？) はインクリメントされる
    * object じゃなくて内部データ構造であり、参照カウンタインクリメントされるということはポインタの指し先は a が生きれてば勝手に解放されない・・・か？
  
