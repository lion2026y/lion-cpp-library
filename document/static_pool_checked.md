# static_pool_checked

[Source](../src/pool/static_pool_checked.hpp)

## 概要

[static_pool](static_pool.md) の `LION_DEBUG` 定義時の実体（誤用検知版）。通常版と同じ API を持つが、速度を度外視して、プールの誤った使い方を `assert` で即座に停止させることに特化している。`lion/static_pool.hpp` を include しているコードは、`LION_DEBUG` を定義して再コンパイルするだけでこちらに切り替わる。

このファイルは直接 include せず、`lion/static_pool.hpp` 経由で使う。API とその使い方は [static_pool](static_pool.md) を参照。ここでは検知版固有の仕様を説明する。

主な目的は、通常版では素通りしてしまう「free したスロットが再確保された後に、古いハンドルでアクセスする」誤用の検知である。

```cpp
auto a = pool.alloc(1);
pool.free(a);
auto b = pool.alloc(2); // a と同じスロットが再利用される
pool[a];                // 通常版では b の要素にそのままアクセスできてしまう。検知版は assert で停止する
```

検知は `assert` で行うので、`NDEBUG` を定義すると何も検知しない。

## 検知できる誤用

| 誤用 | 検知するタイミング | `assert` のメッセージ |
|---|---|---|
| `NIL` へのアクセス・`NIL` の `free` | その操作 | `static_pool: NIL handle.` |
| 確保していない・範囲外のハンドルの使用 | その操作 | `static_pool: invalid handle (never allocated / out of range / used after clear).` |
| free 後のアクセス（use after free）・二重 free | その操作 | `static_pool: freed slot (use after free / double free).` |
| free したスロットが再確保された後に、古いハンドルでアクセス・`free` する | その操作 | `static_pool: stale handle (slot was freed and reallocated).` |
| `clear` 前に得たハンドルの使用 | その操作 | スロットがまだ再び切り出されていなければ `invalid handle ...`、切り出された後なら `stale handle ...` |
| free 済みの領域へ、古いポインタ・参照経由で書き込む | そのスロットが次に再確保されるとき | `static_pool: write to freed slot (dangling pointer).` |
| 容量 `N` を超える `alloc` | その `alloc` | `static_pool: capacity N exceeded.` |
| 非 trivial な `T` の生存要素が残った状態での `clear`（`fast == false` 時） | その `clear` | `static_pool::clear would skip live destructors; free all or set fast.` |

このほか、1 つのスロットで $2^{32} - 1$ 回 `free` すると世代が尽きて `static_pool: generation exhausted ...` で停止する（実用上は起こらない）。

## 検知できない誤用

ハンドル経由のアクセスしか監視できないため、次の誤用は検知できない。

- free した後、そのスロットが再確保されないうちに、古いポインタ・参照で読み書きする。毒値の検査は再確保の直前にしか行わないので、再確保されなければ検知されない（読み出した値は毒値 `0xDD` で埋まったゴミになる）。
- 再確保済みのスロットへ、古いポインタ・参照で書き込む。スロットは生存中で毒値も残っていないので、新しい持ち主のデータが黙って壊れる。

どちらも `operator[]` / `ptr` が返した `T&` / `T*` を `free` をまたいで持ち回ることが原因なので、参照は使い捨てにし、持ち回るならハンドルで持つこと。

## 仕組み

- **世代（generation）付きハンドル**：ハンドルを「スロットの index」と「世代」の組にし、スロットごとに現在の世代を記録する。`free` のたびにそのスロットの世代を進めるので、同じスロットが再確保されても古いハンドルの世代とは一致しない。これで再利用後の古いハンドルを検知する。`clear` でも生存中のスロットの世代を進めるので、`clear` 前のハンドルも検知できる。世代は index とは別の 32 bit 整数なので、`N` を大きくしても世代が尽きにくい。
- **スロットの状態**：スロットごとに「未使用・生存中・free 済み」のどれかを記録し、free 済みのスロットへのアクセスを use after free / 二重 free として報告する。
- **毒値**：`free` したスロットの領域を `0xDD` で埋め、再確保の直前に毒値が無傷かを検査する。壊れていれば、free した後に古いポインタ経由で書き込まれたと分かる。
- **空きリストを領域の外に置く**：通常版のように空きリストを解放済みの領域に書き込む（intrusive）と、古いポインタからの書き込みで空きリスト自体が壊れ、検知どころか動作が破綻する。そのため検知版は空きスロットの index を別の配列に積む。
- 通常版と同じく、プール全体は trivially destructible に保っている（グローバルなプールが終了時に破棄されない性質は変わらない）。

## 通常版との違い

| 項目 | 通常版（[static_pool_fast](static_pool_fast.md)） | 検知版 |
|---|---|---|
| ハンドル `id_type`（= `static_pool_id`） | `uint32_t`（4B） | `struct { uint32_t idx, gen; }`（8B） |
| `NIL` | `uint32_t(-1)` | `{uint32_t(-1), uint32_t(-1)}` |
| 1 スロットあたりのサイズ | `max(sizeof(T), 4)` バイト | `sizeof(T) + 9` バイト（世代 4B・状態 1B・空きスロットの置き場 4B） |
| 既定の `N` でのプールのサイズ | `T = int` で 4MiB、16B の構造体で 16MiB | `T = int` で 13MiB、16B の構造体で 25MiB |
| コンストラクタ | $O(1)$ | $O(N)$ |
| `alloc` / `free` | $O(1)$ | $O(\mathrm{sizeof}(T))$（毒値の検査・書き込み） |
| `clear` | $O(1)$ | $O(\text{これまでに切り出したスロット数})$ |
| 再利用の順序 | 直近に `free` したスロットから（LIFO） | 保証しない |

ハンドルが 8B になるので、ハンドルを持つ要素型（木のノードなど）も通常版より大きくなる。

検知版の `static_pool_id` も順序比較（`<=>`）と `std::hash` を持つので、通常版の `uint32_t` と同じように `set` / `map` / `unordered_set` のキーやソートに使える（大小に意味はない）。差し替えただけで利用側のコードが壊れないようにするためである。
