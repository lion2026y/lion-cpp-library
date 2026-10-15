# weight_balanced_leafy_tree (Weight Balanced Leafy Tree)

[Source](../src/self_balancing_BST/WeightBalancedLeafyTree.hpp)

## 概要

重み平衡 leafy 木。要素は葉にだけ置き、内部ノードは部分木の集約値（モノイド積）だけを持つ二分木を、部分木の葉数（重み）で平衡させる。1 つのテンプレートで次の 2 通りに使える。

| モード | 指定 | できること |
|---|---|---|
| 列（sequence） | `RawKey = void` | 位置（index）で要素を管理する列。index 指定の挿入・削除・更新、区間積、木上二分探索、列の分割・連結 |
| 順序付き map（ordered） | `RawKey` に key の型 | key が一意な順序付き map。key 指定の挿入・削除・検索、key 範囲と index 範囲の区間積、木上二分探索、key による分割、map 同士の併合（`meld`） |

```cpp
using Seq = weight_balanced_leafy_tree<Mode::plain, void, long long>; // 列
using Map = weight_balanced_leafy_tree<Mode::plain, int, long long>;  // key = int, 値 = long long の map
```

競プロでの主な用途：

- 列の任意位置への挿入・削除と区間積（`insert` / `erase` / `prod`）
- 列の切り貼り（`split_off` / `concat` / `insert(k, tree)` / `erase(l, r)`）
- key の範囲の積や、key 順で $k$ 番目の要素を扱う順序付き map（`prod(kl, kr)` / `key_at` / `lower_bound`）
- map 同士の併合（`meld`）

計算量は特記がなければ $O(\log n)$（$n$ は木の要素数。テンプレートパラメータの `N` とは別）。重み平衡により、木の高さは $\log_{7/5} n \approx 2.06 \log_2 n$ 以下に保たれる。

### モノイドの要件

値の型 `Value` に対して、`Q` と `Q_e` が $(\mathrm{Value}, Q, e)$ のモノイドを成すこと、すなわち

- 結合律：$Q(Q(a, b), c) = Q(a, Q(b, c))$
- 単位元：$Q(a, e) = Q(e, a) = a$（$e$ = `Q_e()`）

が成り立つことを要求する。可換である必要はない。区間積は常に列の順（map では key の昇順）に左から畳み込む。

### key の要件（順序付き map）

- key は一意（map と同じ意味）。同じ key の要素は 1 つしか存在できない。同じ key への `upsert` は値を置き換え、`meld` で key が重なったときは統合関数で値をまとめる。
- `comp` は key の狭義弱順序。`comp(a, b)` と `comp(b, a)` がともに偽なら同じ key とみなす。以下、key の大小は `comp` による順序を表す。
- `Key` は trivially destructible・trivially copy constructible・copy assignable・default constructible であること（`int` / `long long` / `pair<int, int>` などは可、`string` は不可）。

### 注意点

- 分割・連結の API はモードによって異なる。列は `split_off(k)` / `concat` / `insert(k, tree)` / `erase(l, r)`、map は `split_off(key)` / `meld`。map に `concat` は無く、map 同士は key の重なりを許す `meld` で併合する。
- `concat` / `insert(k, tree)` / `meld` は引数の木から要素を移す。呼び出し後、引数の木は空になる。
- map への挿入は `upsert`（無ければ挿入、あれば置換）または `modify_or` で行う。`insert` という名前の関数は無い。`get(key)` / `set(key, v)` / `modify(key, f)` は key が存在することが前提で、無ければ `assert` で停止する。
- `max_right` / `min_left` の引数の順序は `(f, l)` / `(f, r)` で、ACL の segtree とは逆。
- メモリは [static_pool](static_pool.md) から確保する。pool は `static` メンバなので、**テンプレート引数が同じ木はすべて 1 つの pool を共有する**（テンプレート引数が異なる木はそれぞれ別の pool を持つ）。要素数 $n$ の木は $2n - 1$ 個のノードを使うので、同時に存在する全ての木の要素数の合計を $n_{\mathrm{total}}$ として、`N` は $2 n_{\mathrm{total}}$ 程度（分割・連結の途中で使う一時的なノードの分、少し余裕を持たせる）にする。既定の `N = (1 << 20) - 1` では約 52 万要素まで扱える。
- pool のサイズは `N × sizeof(node)` で、静的領域に確保される。既定の `N` での目安は次のとおり。

  | 木 | `sizeof(node)` | pool のサイズ |
  |---|---|---|
  | 列, `Value = int` | 16B | 16MiB |
  | 列, `Value = long long` | 24B | 24MiB |
  | map, `Key = int`, `Value = int` | 24B | 24MiB |
  | map, `Key = int`, `Value = long long` | 32B | 32MiB |
  | map, `Key = long long`, `Value = long long` | 40B | 40MiB |

  `LION_DEBUG` 定義時はハンドルが 8B になり、pool も誤用検知用の領域を持つため 1.4〜2 倍程度になる（例：`Value = long long` の列で 41MiB）。
- `LION_DEBUG` を定義すると、pool が誤用検知版（[static_pool_checked](static_pool_checked.md)）に切り替わるうえ、変更操作のたびに `validate()` が木の全体を検査する。このとき変更操作 1 回が $O(n)$ になるので、大きな入力を試すときは `LION_DEBUG` を外す。
- 遅延評価（`Mode::lazy` / `Mode::beats`）は未実装。`Mode::plain` 以外を指定するとコンパイルエラーになる。

## テンプレートパラメータ

```cpp
template<Mode mode = Mode::plain, class RawKey = void,
         class Value = conditional_t<is_void_v<RawKey>, long long, RawKey>,
         auto Q = plus<Value>{}, auto Q_e = bbst::default_e<Value>,
         auto comp = less<RawKey>{}, size_t N = (1 << 20) - 1>
struct weight_balanced_leafy_tree;
```

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `mode` | `Mode::plain` | 集約の遅延レベル。現状は `Mode::plain`（遅延評価なし）のみ対応 |
| `RawKey` | `void` | key の型。`void` なら列、それ以外なら順序付き map になる |
| `Value` | `RawKey`（`RawKey = void` なら `long long`） | 値の型（モノイドの台集合）。`void` は不可 |
| `Q` | `plus<Value>{}` (= `a + b`) | モノイド演算。`Value(Value, Value)` として呼び出せること |
| `Q_e` | `bbst::default_e<Value>` (= `Value{}`) | 単位元 $e$ を返す関数。`Value()` として呼び出せること |
| `comp` | `less<RawKey>{}` | key の比較関数（狭義弱順序）。`bool(Key, Key)` として呼び出せること。列では使わない |
| `N` | `(1 << 20) - 1` | ノード用 pool の容量（同時に存在できるノード数の上限）。$5N$ が `int` に収まること |

`Q` / `Q_e` / `comp` は非型テンプレートパラメータなので、関数ポインタ・キャプチャの無いラムダ・関数オブジェクトのいずれも渡せる。`Value` の既定が `RawKey` なのは set としての利用を想定したもので、`upsert(k, k)` で挿入すれば `prod` で key の総和を取れる。

```cpp
// 区間 min の列
using MinSeq = weight_balanced_leafy_tree<Mode::plain, void, int,
    [](int a, int b) { return min(a, b); }, [] { return INT_MAX; }>;
// key の降順に並ぶ map
using DescMap = weight_balanced_leafy_tree<Mode::plain, int, long long,
    plus<long long>{}, bbst::default_e<long long>, greater<int>{}>;
```

## 内部型・静的メンバ

### Mode

```cpp
enum class Mode { plain, lazy, beats }; // bbst::Mode。グローバルに using 済み
```

テンプレートパラメータ `mode` に渡す、集約の遅延レベル。`plain` は遅延評価なし。`lazy`（区間作用）と `beats` は未実装。

### ID

```cpp
struct ID { int id; explicit ID(int i); }; // bbst::ID。Tree::ID でも参照できる
```

順序付き map で、引数が key ではなく index（key の昇順で何番目か、0-indexed）であることを型で区別するためのラッパ。`get` / `set` / `modify` / `erase` / `prod` / `max_right` / `min_left` の index 版で使う。コンストラクタが `explicit` なので、整数から暗黙には変換されない。

```cpp
m.prod(10, 20);                 // 10 <= key < 20 の積
m.prod(Map::ID(1), Map::ID(3)); // key の昇順で 1, 2 番目の要素の積
```

### ordered

```cpp
static constexpr bool ordered;
```

順序付き map なら `true`、列なら `false`。

### Key

```cpp
using Key = conditional_t<ordered, RawKey, monostate>;
```

key の型。列では使われず、`monostate` になる。

### fast

```cpp
static inline bool& fast; // ノード用 pool の static_pool::fast への参照
```

破棄処理を省いて高速化するフラグ（既定 `false`）。同じテンプレート引数の全ての木で共有される。`true` にすると次の処理を省く。

- デストラクタと代入演算子で、元の木のノードを pool に返す処理
- `Value` が非 trivial な破棄子を持つ場合の、破棄子の呼び出し

プログラムの終了間際に、大きな木の破棄にかかる時間を省きたいときに使う。`true` の間は破棄した木のノードが pool に戻らないので、木を作っては捨てるループで使うと pool の容量 `N` を使い切って停止する。`clear()` は `fast` によらずノードを pool に返す。

## コンストラクタ

以下、`Tree` は `weight_balanced_leafy_tree<...>` の別名、`t` はそのオブジェクトとする。

```cpp
Tree t;                             // (1)
Tree t(vector<Value> const& v);     // (2) 列のみ
Tree t(vector<pair<Key, Value>> v); // (3) 順序付き map のみ
Tree t(Tree const& ot);             // (4)
Tree t(Tree&& ot);                  // (5)
```

- (1) 空の木を作る。
- (2) 列 `v` から構築する。
- (3) key と値の組の配列 `v` から構築する。`v` は key の順に並んでいなくてよい（内部で `stable_sort` する）。
- (4) `ot` を深くコピーする（ノードは共有しない）。
- (5) `ot` の中身を引き継ぐ。`ot` は空になる。

コピー代入・ムーブ代入も (4)・(5) と同様で、代入先が元々持っていた要素は解放される。デストラクタは木の全ノードを pool に返す（`fast == true` なら返さない）。

**制約**

- (3) `v` の key は互いに異なること（重複していると `assert` で停止）

**計算量**

- (1), (5) $O(1)$
- (2), (4) $O(n)$
- (3) $O(n \log n)$

**使用例**

```cpp
Seq a(vector<long long>{3, 1, 4, 1, 5});
Map m(vector<pair<int, long long>>{{30, 3}, {10, 1}, {20, 2}}); // {10:1, 20:2, 30:3}
Seq b = a; // 深いコピー。b を変更しても a は変わらない
```

## 関数

以下の使用例では次の別名を使う。

```cpp
using Seq = weight_balanced_leafy_tree<Mode::plain, void, long long>;
using Map = weight_balanced_leafy_tree<Mode::plain, int, long long>;
```

### 共通

#### size

```cpp
int t.size() const
```

要素数を返す。

**計算量**

- $O(1)$

---

#### empty

```cpp
bool t.empty() const
```

木が空なら `true` を返す。

**計算量**

- $O(1)$

---

#### all_prod

```cpp
Value t.all_prod() const
```

全要素の値の積 $Q(v_0, v_1, \ldots, v_{n-1})$（列の順、map では key の昇順）を返す。空なら $e$ を返す。

**計算量**

- $O(1)$

**使用例**

```cpp
Seq t(vector<long long>{3, 1, 4});
t.all_prod(); // 8
```

---

#### clear

```cpp
void t.clear()
```

全要素を削除し、ノードを pool に返す。`fast` の値によらず返す。

**計算量**

- $O(n)$

---

### 列（`RawKey = void`）

#### get

```cpp
Value const& t.get(int k) const
```

index `k` の要素の値を返す。戻り値は木の中の値への参照で、木を変更すると無効になりうる。

**制約**

- $0 \leq k < \mathrm{size()}$

**計算量**

- $O(\log n)$

**使用例**

```cpp
Seq t(vector<long long>{3, 1, 4});
t.get(2); // 4
```

---

#### set

```cpp
void t.set(int k, Value const& v)
```

index `k` の要素の値を `v` に変更する。

**制約**

- $0 \leq k < \mathrm{size()}$

**計算量**

- $O(\log n)$

---

#### modify

```cpp
void t.modify(int k, F const& f)
```

index `k` の要素の値 $x$ を $f(x)$ に置き換える。`f` は `Value(Value)` として呼び出せること。

**制約**

- $0 \leq k < \mathrm{size()}$

**計算量**

- $O(\log n)$

**使用例**

```cpp
t.modify(1, [](long long x) { return x * 2; });
```

---

#### insert

```cpp
void t.insert(int k, Value const& v)
```

index `k` の手前に `v` を挿入する。挿入後、`v` は `k` 番目の要素になる。`k = size()` なら末尾に追加する。

**制約**

- $0 \leq k \leq \mathrm{size()}$

**計算量**

- $O(\log n)$

**使用例**

```cpp
Seq t(vector<long long>{3, 1, 4});
t.insert(1, 9);        // {3, 9, 1, 4}
t.insert(t.size(), 5); // {3, 9, 1, 4, 5}
```

---

#### erase

```cpp
void t.erase(int k)
```

index `k` の要素を削除する。

**制約**

- $0 \leq k < \mathrm{size()}$

**計算量**

- $O(\log n)$

---

#### prod

```cpp
Value t.prod(int l, int r) const
```

区間 $[l, r)$ の積 $Q(v_l, \ldots, v_{r-1})$ を返す。$l = r$ なら $e$ を返す。

**制約**

- $0 \leq l \leq r \leq \mathrm{size()}$

**計算量**

- $O(\log n)$

**使用例**

```cpp
Seq t(vector<long long>{3, 1, 4, 1, 5});
t.prod(1, 4); // 1 + 4 + 1 = 6
```

---

#### max_right

```cpp
int t.max_right(F const& f) const
int t.max_right(F const& f, int l) const
```

$f(\mathrm{prod}(l, r))$ が `true` となる最大の $r$ を返す。`l` を省略すると $l = 0$。`f` は `bool(Value)` として呼び出せること。ACL の `segtree::max_right` と同じだが、引数の順序が逆である点に注意。

**制約**

- $0 \leq l \leq \mathrm{size()}$
- $f(e) = $ `true`
- `f` が単調であること（ある区間で `true` なら、それに含まれる同じ左端の区間でも `true`）

**計算量**

- $O(\log n)$

**使用例**

```cpp
Seq t(vector<long long>{1, 2, 3, 4, 5});
auto f = [](long long s) { return s <= 6; };
t.max_right(f);    // 3 (1+2+3 = 6)
t.max_right(f, 1); // 3 (2+3 = 5, 2+3+4 = 9)
```

---

#### min_left

```cpp
int t.min_left(F const& f) const
int t.min_left(F const& f, int r) const
```

$f(\mathrm{prod}(l, r))$ が `true` となる最小の $l$ を返す。`r` を省略すると $r = \mathrm{size()}$。`f` は `bool(Value)` として呼び出せること。ACL の `segtree::min_left` と同じだが、引数の順序が逆である点に注意。

**制約**

- $0 \leq r \leq \mathrm{size()}$
- $f(e) = $ `true`
- `f` が単調であること（ある区間で `true` なら、それに含まれる同じ右端の区間でも `true`）

**計算量**

- $O(\log n)$

**使用例**

```cpp
Seq t(vector<long long>{1, 2, 3, 4, 5});
auto g = [](long long s) { return s <= 9; };
t.min_left(g);    // 3 (4+5 = 9)
t.min_left(g, 4); // 1 (2+3+4 = 9)
```

---

#### get_vec

```cpp
vector<Value> t.get_vec() const
```

全要素を列の順に並べた `vector` を返す。

**計算量**

- $O(n)$

---

### 列の分割・連結（`RawKey = void`）

#### split_off

```cpp
Tree t.split_off(int k)
```

先頭 `k` 個の要素を `t` に残し、index `k` 以降の要素を新しい木として返す。

**制約**

- $0 \leq k \leq \mathrm{size()}$

**計算量**

- $O(\log n)$

**使用例**

```cpp
Seq t(vector<long long>{0, 1, 2, 3, 4});
Seq u = t.split_off(2); // t = {0, 1}, u = {2, 3, 4}
```

---

#### concat

```cpp
void t.concat(Tree& ot)
```

`ot` の要素を `t` の末尾に連結する。`ot` は空になる。

**制約**

- `ot` は `t` 自身でないこと

**計算量**

- $O(\log (n + m))$（$m$ は `ot` の要素数）

**使用例**

```cpp
Seq t(vector<long long>{0, 1}), u(vector<long long>{2, 3});
t.concat(u); // t = {0, 1, 2, 3}, u は空
```

---

#### insert（木の挿入）

```cpp
void t.insert(int k, Tree& ot)
```

`ot` の列を丸ごと index `k` の手前に差し込む。`ot` は空になる。

**制約**

- $0 \leq k \leq \mathrm{size()}$
- `ot` は `t` 自身でないこと

**計算量**

- $O(\log (n + m))$（$m$ は `ot` の要素数）

**使用例**

```cpp
Seq t(vector<long long>{0, 1, 2}), w(vector<long long>{7, 8});
t.insert(1, w); // t = {0, 7, 8, 1, 2}, w は空

// split_off と組み合わせた列の回転
Seq u = t.split_off(3); // t = {0, 7, 8}, u = {1, 2}
t.insert(0, u);         // t = {1, 2, 0, 7, 8}
```

---

#### erase（区間削除）

```cpp
void t.erase(int l, int r)
```

index が $[l, r)$ の要素をまとめて削除する。

**制約**

- $0 \leq l \leq r \leq \mathrm{size()}$

**計算量**

- $O(\log n + (r - l))$

**使用例**

```cpp
Seq t(vector<long long>{0, 1, 2, 3, 4});
t.erase(1, 3); // {0, 3, 4}
```

---

### 順序付き map：key 指定（`RawKey` ≠ `void`）

#### lower_bound / upper_bound

```cpp
int t.lower_bound(Key const& k) const
int t.upper_bound(Key const& k) const
```

`lower_bound` は key $< k$ の要素数、`upper_bound` は key $\leq k$ の要素数を返す。すなわち key の昇順で、$k$ 以上（`upper_bound` は $k$ より大きい）の最初の要素の index を返す。該当が無ければ `size()` を返す。`std::map` と違い、イテレータではなく index を返す。

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m(vector<pair<int, long long>>{{1, 10}, {3, 30}, {5, 50}});
m.lower_bound(3); // 1
m.upper_bound(3); // 2
m.lower_bound(4); // 2
```

---

#### contains

```cpp
bool t.contains(Key const& k) const
```

key `k` の要素が存在すれば `true` を返す。

**計算量**

- $O(\log n)$

---

#### get

```cpp
Value const& t.get(Key const& k) const
```

key `k` の要素の値を返す。戻り値は木の中の値への参照で、木を変更すると無効になりうる。

**制約**

- key `k` の要素が存在すること（無ければ `assert` で停止）

**計算量**

- $O(\log n)$

---

#### get_or

```cpp
Value t.get_or(Key const& k, Value const& def = e) const
```

key `k` の要素の値を返す。無ければ `def`（省略時は $e$）を返す。

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m(vector<pair<int, long long>>{{1, 10}});
m.get_or(1);     // 10
m.get_or(2);     // 0 (= e)
m.get_or(2, -1); // -1
```

---

#### set

```cpp
void t.set(Key const& k, Value const& v)
```

key `k` の要素の値を `v` に変更する。key が無い場合も挿入したいなら `upsert` を使う。

**制約**

- key `k` の要素が存在すること（無ければ `assert` で停止）

**計算量**

- $O(\log n)$

---

#### modify

```cpp
void t.modify(Key const& k, F const& f)
```

key `k` の要素の値 $x$ を $f(x)$ に置き換える。`f` は `Value(Value)` として呼び出せること。

**制約**

- key `k` の要素が存在すること（無ければ `assert` で停止）

**計算量**

- $O(\log n)$

---

#### modify_or

```cpp
void t.modify_or(Key const& k, F const& f)
```

key `k` の要素があればその値 $x$ を $f(x)$ に置き換え、無ければ key `k`・値 $f(e)$ の要素を挿入する。`f` は `Value(Value)` として呼び出せること。

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map cnt; // key ごとの出現回数
cnt.modify_or(7, [](long long x) { return x + 1; }); // {7:1}
cnt.modify_or(7, [](long long x) { return x + 1; }); // {7:2}
```

---

#### upsert

```cpp
void t.upsert(Key const& k, Value const& v)
```

key `k` の要素が無ければ $(k, v)$ を挿入し、あれば値を `v` に置き換える。`k` や `v` に、`key_at` / `get` が返した木の中への参照を渡してもよい。

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m;
m.upsert(5, 50);
m.upsert(1, 10);
m.upsert(5, 55); // 置換。m = {1:10, 5:55}
```

---

#### erase

```cpp
bool t.erase(Key const& k)
```

key `k` の要素を削除する。削除したら `true`、key が無ければ何もせず `false` を返す。

**計算量**

- $O(\log n)$

---

#### split_off

```cpp
Tree t.split_off(Key const& s)
```

key $\geq s$ の要素を切り離して新しい木として返す。`t` には key $< s$ の要素だけが残る。

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m(vector<pair<int, long long>>{{10, 1}, {20, 2}, {30, 3}, {40, 4}});
Map u = m.split_off(25); // m = {10:1, 20:2}, u = {30:3, 40:4}
```

---

#### prod

```cpp
Value t.prod(Key const& kl, Key const& kr) const
```

$kl \leq$ key $< kr$ を満たす要素の値の積（key の昇順）を返す。該当する要素が無いとき、および $kl \geq kr$ のときは $e$ を返す。

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m(vector<pair<int, long long>>{{1, 10}, {3, 30}, {5, 50}});
m.prod(2, 6); // 30 + 50 = 80
```

---

#### max_right

```cpp
int t.max_right(F const& f, Key const& kl) const
```

key $\geq kl$ の最初の要素の index を $l$（= `lower_bound(kl)`）として、$f(\mathrm{prod}(\mathrm{ID}(l), \mathrm{ID}(r)))$ が `true` となる最大の $r$ を返す。戻り値は key ではなく index。`lower_bound` と二分探索を 1 回の降下でまとめて行う。`f` は `bool(Value)` として呼び出せること。

**制約**

- $f(e) = $ `true`
- `f` が単調であること

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m(vector<pair<int, long long>>{{10, 1}, {20, 2}, {30, 3}, {40, 4}});
auto f = [](long long s) { return s <= 5; };
int r = m.max_right(f, 15); // 3 (key 20, 30 の和が 5。40 まで含めると 9)
m.key_at(r - 1);            // 30 (条件を満たす最後の要素の key)
```

---

#### min_left

```cpp
int t.min_left(F const& f, Key const& kr) const
```

key $< kr$ の要素数を $r$（= `lower_bound(kr)`）として、$f(\mathrm{prod}(\mathrm{ID}(l), \mathrm{ID}(r)))$ が `true` となる最小の $l$ を返す。戻り値は index。`f` は `bool(Value)` として呼び出せること。

**制約**

- $f(e) = $ `true`
- `f` が単調であること

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m(vector<pair<int, long long>>{{10, 1}, {20, 2}, {30, 3}, {40, 4}});
auto f = [](long long s) { return s <= 5; };
m.min_left(f, 35); // 1 (key 20, 30 の和が 5。10 まで含めると 6)
```

---

#### meld

```cpp
void t.meld(Tree& ot, F const& f = Q)
```

`ot` の全要素を `t` に併合する。両方に同じ key があれば、2 つの値を `f` でまとめた 1 要素にする。`ot` は空になる。

**制約**

- `f` は `Value(Value, Value)` として呼び出せること（省略時は `Q`）
- `f` は可換であること（内部で木を入れ替えながら併合するため、`f` に渡す引数の順序は不定）

**計算量**

- $O(m \log (n / m + 1))$（$n$, $m$ は 2 つの木の要素数の大きい方と小さい方）

**使用例**

```cpp
Map a(vector<pair<int, long long>>{{1, 10}, {3, 30}});
Map b(vector<pair<int, long long>>{{2, 20}, {3, 300}});
a.meld(b); // a = {1:10, 2:20, 3:330}, b は空

Map c(vector<pair<int, long long>>{{3, 5}, {4, 40}});
a.meld(c, [](long long x, long long y) { return max(x, y); }); // a = {1:10, 2:20, 3:330, 4:40}
```

---

#### get_vec_pair

```cpp
vector<pair<Key const&, Value const&>> t.get_vec_pair() const
```

全要素の key と値の組を key の昇順に並べた `vector` を返す。各要素は木の中の key と値への参照で、木を変更すると無効になりうる。

**計算量**

- $O(n)$

**使用例**

```cpp
for(auto [k, v]: m.get_vec_pair()) cout << k << ":" << v << " ";
```

---

### 順序付き map：index 指定（`RawKey` ≠ `void`）

index は key の昇順で何番目か（0-indexed）を表す。key 版と区別するため、`key_at` 以外は index を [`ID`](#id) で包んで渡す。

#### get

```cpp
Value const& t.get(ID i) const
```

index `i` の要素の値を返す。

**制約**

- $0 \leq i.\mathrm{id} < \mathrm{size()}$

**計算量**

- $O(\log n)$

---

#### key_at

```cpp
Key const& t.key_at(int i) const
```

index `i` の要素の key を返す。引数は `ID` ではなく `int`。

**制約**

- $0 \leq i < \mathrm{size()}$

**計算量**

- $O(\log n)$

**使用例**

```cpp
Map m(vector<pair<int, long long>>{{30, 3}, {10, 1}, {20, 2}});
m.key_at(0);       // 10
m.get(Map::ID(0)); // 1
```

---

#### set

```cpp
void t.set(ID i, Value const& v)
```

index `i` の要素の値を `v` に変更する。key は変わらない。

**制約**

- $0 \leq i.\mathrm{id} < \mathrm{size()}$

**計算量**

- $O(\log n)$

---

#### modify

```cpp
void t.modify(ID i, F const& f)
```

index `i` の要素の値 $x$ を $f(x)$ に置き換える。`f` は `Value(Value)` として呼び出せること。

**制約**

- $0 \leq i.\mathrm{id} < \mathrm{size()}$

**計算量**

- $O(\log n)$

---

#### erase

```cpp
void t.erase(ID i)
```

index `i` の要素を削除する。

**制約**

- $0 \leq i.\mathrm{id} < \mathrm{size()}$

**計算量**

- $O(\log n)$

---

#### prod

```cpp
Value t.prod(ID l, ID r) const
```

index が $[l, r)$ の要素の値の積を返す。$l = r$ なら $e$ を返す。

**制約**

- $0 \leq l.\mathrm{id} \leq r.\mathrm{id} \leq \mathrm{size()}$

**計算量**

- $O(\log n)$

---

#### max_right

```cpp
int t.max_right(F const& f) const
int t.max_right(F const& f, ID l) const
```

$f(\mathrm{prod}(l, r))$ が `true` となる最大の $r$ を返す。`l` を省略すると $l = 0$。仕様は列の [max_right](#max_right) と同じ。

**制約**

- $0 \leq l.\mathrm{id} \leq \mathrm{size()}$
- $f(e) = $ `true`
- `f` が単調であること

**計算量**

- $O(\log n)$

---

#### min_left

```cpp
int t.min_left(F const& f) const
int t.min_left(F const& f, ID r) const
```

$f(\mathrm{prod}(l, r))$ が `true` となる最小の $l$ を返す。`r` を省略すると $r = \mathrm{size()}$。仕様は列の [min_left](#min_left) と同じ。

**制約**

- $0 \leq r.\mathrm{id} \leq \mathrm{size()}$
- $f(e) = $ `true`
- `f` が単調であること

**計算量**

- $O(\log n)$

---

### デバッグ用

#### height

```cpp
int t.height() const
```

木の高さ（根から最も深い葉までのノード数）を返す。空の木なら 0、要素 1 つなら 1。

**計算量**

- $O(n)$

---

#### validate

```cpp
void t.validate() const
```

`LION_DEBUG` 定義時のみ、木が全ての不変条件（部分木の重み、集約値、重み平衡、key の狭義単調増加、内部ノードが持つ key の情報）を満たすかを `assert` で検査する。未定義時は何もしない。変更操作の後には自動で呼ばれるので、通常は明示的に呼ぶ必要はない。集約値の検査は `Value` が `==` で比較できる場合のみ行う。

**計算量**

- `LION_DEBUG` 定義時は $O(n)$、未定義時は $O(1)$
