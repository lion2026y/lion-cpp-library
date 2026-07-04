# static_pool

[Source](../src/pool/static_pool.hpp)

## 概要

静的配列と intrusive free list を組み合わせた、index 版のメモリプール。ノードベースのデータ構造（平衡二分探索木の回転・構築など「触りながら確保する」用途）で、動的メモリ確保のコストを避けつつ、確保済みオブジェクトの安定した参照を得るために使う。

特徴：

- **静的配列**：確保済みのオブジェクトは移動しないため、index / 参照が安定する。
- **intrusive free list**：`free` したスロットの領域にそのまま次の空き index を書き込むため、`stack` 等の別コンテナを持たず `alloc`/`free` が数命令で完了する。
- **返り値は `uint32_t`**：ポインタ（8B）より軽く、相互参照が多い構造でキャッシュ効率が良い。無効 index / 空きリスト終端を表す番兵 `NIL`（`= uint32_t(-1)`）を公開している。

**注意点**：

- `buf` を実体として抱えるため `sizeof(static_pool)` は $N \times \mathrm{sizeof(slot)}$ と巨大になる。ローカル変数として宣言するとスタックオーバーフローで RE になるため、**グローバル変数または `new` での確保を前提**とする。
- 同時に生存しうる要素数が `N` を超えると `alloc` の `assert` で停止する。
- 非 trivial な破棄子を持つ型 `T` も扱えるが、その破棄タイミングは `free` / `clear` と静的フラグ `fast` の設定で制御する（後述）。
- 静的メンバ `fast` は同じ `static_pool<T, N>` インスタンス化を共有する点に注意する。

## テンプレートパラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `T` | — | 格納するオブジェクトの型 |
| `N` | `(1 << 20) - 1` | 同時に生存しうる最大要素数。プールが抱える静的配列 `buf` のサイズ |

## 静的メンバ

### fast

```cpp
static inline bool static_pool<T, N>::fast = false;
```

破棄子の実行を省略するかを制御するフラグ。`T` が非 trivial な破棄子を持つ場合のみ意味を持つ。

- `false`（既定）：`free` 時に `T::~T()` を個別に呼ぶ。
- `true`：`free` 時の破棄子呼び出しを省略し、`clear` も生存要素の破棄なしで一括リセットできる。破棄を捨ててよい（`T` がリソースを保持しない）場面での高速化用途。

trivial な破棄子の型では常に破棄処理は行われず、このフラグは影響しない。

### NIL

```cpp
static constexpr uint32_t static_pool<T, N>::NIL = uint32_t(-1);
```

空きリストの終端および無効 index を表す番兵値。`alloc` が返す index と衝突しない。

## コンストラクタ

```cpp
static_pool<T, N> pool;
```

- 空の状態で初期化する。`buf` の各スロットは未構築のまま（起動時に構築コストを払わない）。

**計算量**

- 実質 $O(1)$（`buf` の構築は各スロットの空コンストラクタ N 回だが no-op のため最適化で消える）

**使用例**

```cpp
static_pool<int> pool;              // グローバルに置く
static_pool<Node, 300000> tree;     // N を指定
```

## 関数

### alloc

```cpp
uint32_t pool.alloc(Args&&... args)
```

スロットを 1 つ確保し、その領域に `T(args...)` を placement new で構築して index を返す。空きリストに再利用可能なスロットがあれば優先して使い、なければ配列の未使用領域から切り出す。

**制約**

- 生存要素数が `N` を超えてはならない（超えると `assert` で停止）

**計算量**

- $O(1)$

**使用例**

```cpp
static_pool<pair<int,int>> pool;
uint32_t i = pool.alloc(3, 5);   // pair(3, 5) を構築
```

---

### free

```cpp
void pool.free(uint32_t i)
```

index `i` のスロットを解放し、空きリストの先頭に戻す。`T` が非 trivial な破棄子を持ち、かつ `fast == false` のときは `T::~T()` を呼ぶ。

**制約**

- `i` は `alloc` で得た有効な index であること（二重 free・無効 index に対するガードはない）

**計算量**

- $O(1)$

**使用例**

```cpp
uint32_t i = pool.alloc();
pool.free(i);
```

---

### clear

```cpp
void pool.clear()
```

プール全体を空の状態にリセットし、`buf` を先頭から再利用可能にする。個別の破棄処理は行わない。

**制約**

- 非 trivial な破棄子を持つ `T` の場合、生存要素の破棄漏れを避けるため「全要素が `free` 済み（`size() == 0`）」または「破棄を捨てる意図で `fast == true`」のいずれかであること（違反時は `assert` で停止）

**計算量**

- $O(1)$

**使用例**

```cpp
pool.clear();   // 全 index を無効化して初期状態へ
```

---

### operator[]

```cpp
T& pool[](uint32_t i)
T const& pool[](uint32_t i) const
```

index `i` に対応するオブジェクトへの参照を返す。

**制約**

- `i` は生存中の有効な index であること（範囲チェックはない）

**計算量**

- $O(1)$

**使用例**

```cpp
uint32_t i = pool.alloc(10);
pool[i] = 42;
int x = pool[i];   // 42
```

---

### size

```cpp
size_t pool.size() const
```

現在生存している要素数を返す。

**計算量**

- $O(1)$

**使用例**

```cpp
uint32_t i = pool.alloc();
pool.size();   // 1
pool.free(i);
pool.size();   // 0
```
