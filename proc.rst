Proc
####

RProc
*****

* あるロジックの塊を示す構造

  - mrbc によるコンパイル結果の一つとしてこれを選べる
  - RiteVM に mrb_run() を介して実行させることができる形式

.. code :: c

  struct RProc {
    MRB_OBJECT_HEADER;
    union {
      mrb_irep *irep;
      mrb_func_t func;
    } body;
    struct RClass *target_class;
    struct REnv *env;
  };

* TODO: 各メンバに関して調べておく
