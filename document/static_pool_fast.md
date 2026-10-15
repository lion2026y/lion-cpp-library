# static_pool_fast

[Source](../src/pool/static_pool_fast.hpp)

## 概要

[static_pool](static_pool.md) の通常ビルド（`LION_DEBUG` 未定義）での実体。静的配列と intrusive free list による実装で、`alloc` / `free` / `clear` がすべて $O(1)$（数命令）で済む。その代わり誤用はほとんど検知しない。

このファイルは直接 include せず、`lion/static_pool.hpp` 経由で使う。API とその使い方は [static_pool](static_pool.md) を参照。ここでは通常ビルド固有の仕様と内部構造を説明する。

## 仕様

| 項目 | 内容 |
|---|---|
| ハンドル `id_type`（= `static_pool_id`） | `uint32_t`。スロットの index そのもの |
| `NIL` | `uint32_t(-1)` |
| 1 スロットのサイズ | `max(sizeof(T), 4)` バイト（アラインメントは `max(alignof(T), 4)`） |
| プール全体のサイズ | 約 `N × max(sizeof(T), 4)` バイト（既定の `N` で、`T = int` なら 4MiB、16B の構造体なら 16MiB） |
| コンストラクタ / `alloc` / `free` / `clear` | すべて $O(1)$ |
| 再利用の順序 | 直近に `free` したスロットから再利用する（LIFO）。ただし検知版とは順序が異なるので、これに依存しないこと |
| 検知する誤用 | 容量 `N` の超過と、非 trivial な `T` の生存要素が残った状態での `clear`（ともに `assert`）のみ |

## 内部構造

- **静的配列**：スロットの配列をプールの中に実体として持つ。確保済みの要素は移動しないので、ハンドル・参照が安定する。
- **intrusive free list**：`free` したスロットの領域そのものに次の空きスロットの index を書き込み、空きスロットを連結リストでつなぐ。別のコンテナ（`stack` など）を持たないので、`alloc` / `free` が数命令で済む。スロットのサイズを `max(sizeof(T), 4)` にしているのは、`sizeof(T) < 4` でも index を書き込めるようにするため。
- **未使用領域からの切り出し**：空きリストが空なら、配列のまだ使っていない領域の先頭から 1 つ切り出す。`clear` は空きリストと切り出し位置を戻すだけなので $O(1)$。
- **生バイト領域のスロット**：スロットを `union` ではなく `std::byte` の配列にして、プール全体を trivially destructible に保っている。そのためグローバルなプールはプログラム終了時に破棄されず（生存期間がプロセスの終了まで続く）、グローバルな木のデストラクタが終了時にプールへ触れても問題にならない。生存中の `T` の破棄は `free` が個別に行う。

## 誤用時の挙動

free 後のアクセス・二重 free・無効なハンドルの使用は検知されず、メモリ破壊を起こす。例えば二重 free をすると空きリストが壊れ、以降の `alloc` が同じスロットを重複して返したり、要素の値を次の空きスロットの index と誤認したりする。

怪しいときは `LION_DEBUG` を定義して再コンパイルし、誤用検知版の [static_pool_checked](static_pool_checked.md) で実行する。
