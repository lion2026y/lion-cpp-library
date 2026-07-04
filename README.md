# cpp_library

競技プログラミング向けC++ライブラリ。

## ディレクトリ構成

```
cpp_library/
├── src/          # ライブラリのソース（ジャンル別）
├── lion/         # インクルード用まとめファイル
└── document/     # 各ライブラリのドキュメント
```

## インクルード方法

全ライブラリ一括インクルード:

```cpp
#include "lion/all.hpp"
```

個別インクルード:

```cpp
#include "lion/hashmap.hpp"
#include "lion/memory_pool.hpp"
// ...
```

## ライブラリ一覧

### algebra

代数的構造・数値型ライブラリ。

- [fraction.hpp](src/algebra/fraction.hpp) \
  有理数（分数）構造体 `frac`。常に既約・分母正の正規形で保持し、四則演算・比較・入出力をサポート。整数との混在演算も可能。 \
  [document](document/fraction.md)

### geometry

2次元幾何演算ライブラリ。

- [line_segment.hpp](src/geometry/line_segment.hpp) \
  点と線分の距離計算など、2次元線分演算を提供する構造体。`point` に依存する。 \
  [document](document/line_segment.md)
- [point.hpp](src/geometry/point.hpp) \
  2次元座標・ベクトル構造体。距離・内積・外積・偏角などの幾何演算に加え、点によるグリッド添字アクセス（`at`/`at_safe`）を提供する。 \
  [document](document/point.md)

### grid

多次元配列・グリッド操作ユーティリティ。

- [cumulative_sum.hpp](src/grid/cumulative_sum.hpp) \
  任意の $d$ 次元配列の累積和をその場で計算するテンプレート関数。二項演算を差し替え可能（default 加算）で、次元数は自動判定する。 \
  [document](document/cumulative_sum.md)
- [grid_transform.hpp](src/grid/grid_transform.hpp) \
  2次元 `random_access_range` の転置・90度回転を行うテンプレート関数。非矩形入力は assert で検出する。 \
  [document](document/grid_transform.md)

### hashmap

ハッシュマップ。

- [hashmap.hpp](src/hashmap/hashmap.hpp) \
  Swissテーブルをベースとした高速ハッシュマップ (`swiss_table`)。カスタムハッシュ・イコール・デフォルト値を指定可能。

### pool

動的メモリ確保のコストを抑えるメモリ管理ユーティリティ。

- [memory_pool.hpp](src/pool/memory_pool.hpp) \
  メモリプール。`malloc` によるチャンク確保とフリーリストを組み合わせ、`alloc`/`free` を均し $O(1)$ で提供する。
- [static_pool.hpp](src/pool/static_pool.hpp) \
  静的配列 + intrusive free list による index 版メモリプール。`alloc`/`free` を $O(1)$ で提供し、安定した index/参照を得られる。グローバル/`new` 確保前提。 \
  [document](document/static_pool.md)

### others

その他のユーティリティ。

- [timer.hpp](src/others/timer.hpp) \
  経過時間をミリ秒単位で計測するタイマー。`steady_clock` ベースで単調増加保証。グローバルインスタンス `cl` が定義済み。 \
  [document](document/timer.md)

### self_balancing_BST

平衡二分探索木。区間演算等をサポートする。

- [lazy_RedBlackTree.hpp](src/self_balancing_BST/lazy_RedBlackTree.hpp) \
  遅延伝播対応の平衡二分探索木（赤黒木）。区間への作用・区間積クエリ・分割合併を $O(\log N)$ でサポート。`memory_pool` に依存する。
- [RedBlackTree.hpp](src/self_balancing_BST/RedBlackTree.hpp) \
  キーと値のペアを管理する平衡二分探索木（赤黒木）。挿入・削除・検索・区間積クエリ・二分探索・木の分割合併を $O(\log N)$ でサポート。 \
  [document](document/RedBlackTree.md)
