# static_pool

[Source](../src/pool/static_pool.hpp)

## 概要

静的配列による index 版のメモリプール。ノードベースのデータ構造（平衡二分探索木の回転・構築など「触りながら確保する」用途）で、動的メモリ確保のコストを避けつつ、確保済みオブジェクトへの安定した参照を得るために使う。`alloc` はポインタではなくハンドル（`id_type`）を返し、要素には `pool[h]` でアクセスする。

特徴：

- **静的配列**：確保済みのオブジェクトは移動しないため、ハンドル・参照が安定する。
- **軽いハンドル**：通常ビルドのハンドルは 4B の index で、ポインタ（8B）より小さい。ノード同士が相互に参照する構造でキャッシュ効率が良い。
- **終了時に破棄されない**：プール自体が trivially destructible なので、グローバルに置いてもプログラム終了時に破棄処理が走らない。グローバルな木のデストラクタが終了時にプールへ触れても、静的オブジェクトの破棄順序に左右されず安全。

### ビルドによる実装の切り替え

`static_pool.hpp` はディスパッチャで、`LION_DEBUG` の有無で実体を切り替える。API は共通なので、再コンパイルするだけで切り替わる。利用側は `lion/static_pool.hpp`（またはこのファイル）だけを include し、実体のファイルを直接 include しないこと。

| ビルド | 実体 | ハンドル `id_type` | 内容 |
|---|---|---|---|
| 通常 | [static_pool_fast](static_pool_fast.md) | `uint32_t` | intrusive free list による高速版。誤用は検知しない |
| `LION_DEBUG` 定義時 | [static_pool_checked](static_pool_checked.md) | 構造体 `{idx, gen}`（8B） | 誤用検知版。free 後のアクセス・二重 free・**free したスロットが再確保された後の古いハンドルの使用**・解放済み領域への書き込みなどを `assert` で検出する（速度は不問） |

### ハンドルの扱い

ハンドルの型は実装によって異なるので、どちらの実装でも動くように次を守る。

- ハンドルは `static_pool<T, N>::id_type`（または `auto`）で受ける。`uint32_t` で受けると検知版でコンパイルできない。
- 要素型 `T` の中にハンドルを持たせる（ノードが子のハンドルを持つなど）ときは、`T` の定義中はまだ `static_pool<T, N>` を参照できないので、`T` に依存しない別名 `static_pool_id` を使う。これは `static_pool<T, N>::id_type` と同じ型である。
- ハンドルは不透明な ID として扱い、比較・代入・`pool` への添字にだけ使う。算術や整数への変換に依存しない。
- 無効値には整数ではなく `NIL` を使う。
- `==` / `<` などの比較と `std::hash` はどちらの実装でも使えるので、`set` / `map` / `unordered_set` のキーやソートには使ってよい。ただし大小関係に意味はない。

```cpp
static_pool<T, N> pool;
using id_type = static_pool<T, N>::id_type; // 通常は uint32_t, LION_DEBUG では構造体
id_type h = pool.alloc();                   // auto でも可
```

### 注意点

- `sizeof(static_pool)` はおよそ $N \times \mathrm{sizeof}(T)$ と巨大になる。ローカル変数として宣言するとスタックオーバーフローで RE になるので、**グローバル変数または `new` での確保を前提**とする。既定の `N` でのサイズの目安は次のとおり。

  | `T` | 通常ビルド | `LION_DEBUG` 定義時 |
  |---|---|---|
  | `int`（4B） | 4MiB | 13MiB |
  | 16B の構造体 | 16MiB | 25MiB |

- 同時に生存できる要素数は `N` まで。超えると `alloc` の `assert` で停止する。
- `operator[]` / `ptr` が返した参照・ポインタは、`free` をまたいで持ち回らない。持ち回るならハンドルで持つ。free したスロットは後の `alloc` で別の要素に再利用されるので、古い参照からの書き込みは新しい持ち主のデータを壊す（検知版でも検知できない場合がある）。
- free したスロットが再利用される順序に依存しない（通常版と検知版で順序が異なる）。
- `T` が非 trivial な破棄子を持つ場合、破棄のタイミングは `free` / `clear` と静的メンバ `fast` で決まる（後述）。
- 静的メンバ `fast` は、同じ `static_pool<T, N>` の全インスタンスで共有される。

## テンプレートパラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `T` | — | 格納するオブジェクトの型 |
| `N` | `(1 << 20) - 1` | 同時に生存しうる最大要素数（プールが抱える静的配列のサイズ）。$N < 2^{32} - 1$ であること |

## 内部型・静的メンバ

### id_type / static_pool_id

```cpp
using static_pool<T, N>::id_type = static_pool_id;
```

`alloc` が返すハンドルの型。通常ビルドでは `uint32_t`、`LION_DEBUG` 定義時は `uint32_t idx, gen` を持つ構造体になる。`static_pool_id` は `T` に依存しない名前空間スコープの別名で、どの `T`, `N` に対しても `static_pool<T, N>::id_type` と同じ型である。

**使用例**

```cpp
struct Node {
  int val;
  static_pool_id next; // Node の定義中は static_pool<Node> を参照できないので static_pool_id を使う
};
static_pool<Node> pool;
using id_type = static_pool<Node>::id_type;     // static_pool_id と同じ型
constexpr id_type NIL = static_pool<Node>::NIL;

id_type head = NIL;
for(int i = 0; i < 3; ++i) head = pool.alloc(i, head); // 2 -> 1 -> 0 の連結リスト
for(id_type u = head; u != NIL; u = pool[u].next) cout << pool[u].val << ' ';
```

---

### NIL

```cpp
static constexpr id_type static_pool<T, N>::NIL;
```

無効なハンドルを表す番兵値。`alloc` が返すハンドルとは衝突しない。通常ビルドでは `uint32_t(-1)`、検知版では `{uint32_t(-1), uint32_t(-1)}`。検知版の `id_type` は整数から構築できないので、番兵は必ず `NIL` で書くこと。

---

### fast

```cpp
static inline bool static_pool<T, N>::fast = false;
```

破棄子の呼び出しを省略するかを制御するフラグ。`T` が非 trivial な破棄子を持つ場合のみ意味を持つ。

- `false`（既定）：`free` 時に `T::~T()` を呼ぶ。
- `true`：`free` 時の破棄子の呼び出しを省略する。また、生存要素が残っていても `clear` できるようになる（生存要素の破棄子は呼ばれない）。破棄を省いてよい（`T` が解放すべきリソースを持たない）場面での高速化用。

`T` の破棄子が trivial なら破棄処理はもともと行われないので、このフラグは影響しない。

## コンストラクタ

```cpp
static_pool<T, N> pool;
```

空のプールを作る。各スロットは未構築のままで、`T` の構築は `alloc` のときに行う。

**計算量**

- 通常ビルド：$O(1)$
- `LION_DEBUG` 定義時：$O(N)$（スロットごとの管理情報を 0 で初期化する）

**使用例**

```cpp
static_pool<int> pool;          // グローバルに置く
static_pool<Node, 300000> tree; // N を指定
```

## 関数

### alloc

```cpp
id_type pool.alloc(Args&&... args)
```

スロットを 1 つ確保し、そこに `T(args...)` を構築してハンドルを返す。free したスロットがあればそれを再利用し、無ければ配列の未使用領域から切り出す。`T` が集成体なら、`pool.alloc(1, h)` のように括弧で各メンバを初期化できる。

**制約**

- `T` が `args...` から構築できること
- 生存要素数が `N` を超えないこと（超えると `assert` で停止）

**計算量**

- 通常ビルド：$O(1)$
- `LION_DEBUG` 定義時：$O(\mathrm{sizeof}(T))$

**使用例**

```cpp
static_pool<pair<int, int>> pool;
auto h = pool.alloc(3, 5); // pair(3, 5) を構築
```

---

### free

```cpp
void pool.free(id_type h)
```

ハンドル `h` のスロットを解放する。`T` が非 trivial な破棄子を持ち、かつ `fast == false` なら `T::~T()` を呼ぶ。解放後は、`h` も `h` から得た参照・ポインタも使えない。

**制約**

- `h` は `alloc` で得た、生存中のハンドルであること。通常ビルドでは検査せず、二重 free などはメモリ破壊を起こす。検知版では `assert` で停止する。

**計算量**

- 通常ビルド：$O(1)$
- `LION_DEBUG` 定義時：$O(\mathrm{sizeof}(T))$

**使用例**

```cpp
auto h = pool.alloc();
pool.free(h);
```

---

### clear

```cpp
void pool.clear()
```

プール全体を空の状態に戻し、配列を先頭から再利用できるようにする。生存中の要素の破棄子は呼ばない。`clear` 前に得たハンドルはすべて無効になる。

**制約**

- `T` が非 trivial な破棄子を持つ場合、全要素が `free` 済み（`size() == 0`）であるか、破棄を省く意図で `fast == true` にしていること（違反すると `assert` で停止）

**計算量**

- 通常ビルド：$O(1)$
- `LION_DEBUG` 定義時：$O(\text{これまでに切り出したスロット数})$

**使用例**

```cpp
pool.clear(); // 全ハンドルを無効化して初期状態へ
```

---

### operator[]

```cpp
T& pool[id_type h]
T const& pool[id_type h] const
```

ハンドル `h` の要素への参照を返す。

**制約**

- `h` は生存中のハンドルであること（通常ビルドでは検査しない。検知版では `assert` で停止する）

**計算量**

- $O(1)$

**使用例**

```cpp
auto h = pool.alloc(10);
pool[h] = 42;
int x = pool[h]; // 42
```

---

### ptr

```cpp
T* pool.ptr(id_type h)
T const* pool.ptr(id_type h) const
```

ハンドル `h` の要素へのポインタを返す。`&pool[h]` と同じ。

**制約**

- `h` は生存中のハンドルであること（通常ビルドでは検査しない。検知版では `assert` で停止する）

**計算量**

- $O(1)$

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
auto h = pool.alloc();
pool.size(); // 1
pool.free(h);
pool.size(); // 0
```
