# interval_set

[Source](../src/others/interval_set.hpp)

## 概要

互いに交わらない半開区間 $[l, r)$ に値 $v$ を持たせて `std::set` で管理する構造体（いわゆる「区間を set で管理するテク」）。区間の上書き（区間代入）・削除、点を含む区間の検索、mex の計算などができる。

接している（$r = l'$ となる）区間どうしで値が同じものは自動的に連結される。そのため保持する区間は常に「同じ値が連続する極大な範囲」になり、表現は一意に定まる。

```cpp
interval_set<int, int> s;
s.update(0, 10, 1); // [0,10):1
s.update(3, 5, 2);  // [0,3):1 [3,5):2 [5,10):1
s.update(5, 7, 2);  // [0,3):1 [3,7):2 [7,10):1  ([3,5):2 とつながる)
s.erase(8, 12);     // [0,3):1 [3,7):2 [7,8):1
```

値を指定せずに `update(l, r)` だけで挿入すると、すべての区間の値が $e$ になる。この場合は、覆われた範囲を極大区間の集合として管理する「区間集合」として使える。

```cpp
interval_set<long long> s;
s.update(1, 3);
s.update(3, 6); // [1,6) にまとまる
s.update(8, 9); // [1,6) [8,9)
```

区間が集合に入るときに `add(l, r, v)`、集合から出るときに `del(l, r, v)` が呼ばれる。分割で範囲が変わる区間も、いったん `del` してから新しい範囲で `add` される。これを使うと、覆われている長さの合計・色ごとの長さ・区間の個数などを差分更新だけで管理できる。

**(重要)** `add` と `del` は互いの逆操作でなければならない。つまり `del(l, r, v)` は `add(l, r, v)` の効果をちょうど打ち消すこと。

```cpp
// 覆われている長さの合計
long long len = 0; // グローバル変数
void add_len(int l, int r, int) { len += r - l; }
void del_len(int l, int r, int) { len -= r - l; }

interval_set<int, int, add_len, del_len> s;
s.update(0, 5);
s.update(3, 8); // len == 8
s.erase(2, 4);  // len == 6
```

```cpp
// 色ごとの長さの合計
array<long long, 3> cnt{}; // グローバル変数
using Paint = interval_set<int, int, [](int l, int r, int v) { cnt[v] += r - l; },
                           [](int l, int r, int v) { cnt[v] -= r - l; }>;

Paint s(vector<int>(10, 0)); // [0,10) を色 0 で初期化
s.update(2, 5, 1);
s.update(4, 7, 2);           // cnt == {5, 2, 3}
```

各操作は償却 $O(\log n)$（$n$ は区間数）で、これに `add` / `del` の呼び出しコストが加わる。1 回の `update` / `erase` で増える区間は高々 2 個なので、削除される区間数の合計は操作回数で抑えられる。

注意点

- `add` / `del` / `f_e` はテンプレート引数（NTTP）なので、キャプチャなしのラムダや関数ポインタ（関数名）など構造的型でなければならない。集計値はグローバル変数などに持たせる。ラムダはラムダ式ごとに別の型になるので、テンプレート引数に直接書く場合は上の `Paint` のように `using` で名前を付けて使い回す。
- `add` / `del` は `update`・`erase` に加えて、構築・コピー・代入・破棄で区間が出入りするときにも呼ばれる（下の「コピー・ムーブ・代入・破棄」を参照）。そのため、生きているすべての interval_set の区間が、ちょうど 1 回ずつ集計値に反映された状態が保たれる。
- 破棄時に `del` が呼ばれるので、`add` / `del` が触る集計値（グローバル変数）は interval_set より前に定義すること。グローバル変数は定義と逆の順に破棄されるため、後に定義した集計値は interval_set より先に破棄され、プログラム終了時に破棄済みの変数を触ってしまう（`long long` などの組み込み型なら問題ない）。
- 区間外の点の値は $e$ として扱われ、`get_val` は $e$ を返す。値が $e$ の区間と区間外は `get_val` では区別できないので、`covered` で判定する。
- `covered(l, r)` と `same(p, q)` は **1 つの区間** に含まれるかを判定する。値の異なる区間にまたがると、すき間なく覆われていても false になる。区間集合として使う場合（値がすべて同じ）は「すべて覆われているか」と一致する。
- `update` / `erase` の後は、変更した範囲にかかっていた区間を指すイテレータや、`get_val` が返した参照は無効になる。ただし、集合内を指す参照（`s[c]`、`it->l` など）を `update` / `erase` の引数に渡すことは問題ない。
- 座標の型 `T` は `<`・`<=`・`>=`・`==` で比較できればよい（`pair` なども使える）。

## テンプレートパラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `T` | — | 座標の型。`<`・`<=`・`>=`・`==` で比較できること |
| `Value` | `int` | 区間の値の型。`==`・`!=` で比較できること |
| `add` | `noop` | 区間が集合に入るときに `add(l, r, v)` として呼ばれる。`void(T, T, Value)` として呼び出せること |
| `del` | `noop` | 区間が集合から出るときに `del(l, r, v)` として呼ばれる。`void(T, T, Value)` として呼び出せること。`add` の逆操作であること |
| `f_e` | `default_e<Value>` | 値 $e$ を返す関数（デフォルトは `Value{}` を返す）。`Value()` として呼び出せること。$e$ は区間外の点の値で、`update` で値を省略したときの値でもある |

`noop` は [noop.hpp](noop.md) で定義される「何もしない関数オブジェクト」。

## 内部型

### node

1 つの区間を表す構造体。イテレータの参照先で、左端の昇順に並ぶ。

| フィールド | 型 | 説明 |
|---|---|---|
| `l` | `T` | 左端（含む） |
| `r` | `T` | 右端（含まない）。常に $l < r$ |
| `v` | `Value` | 区間の値 |

構造化束縛 `auto& [l, r, v] = *it` で分解できる。

### Itr

```cpp
using Itr = set<node, comp>::iterator;
```

`node` を指す読み取り専用のイテレータ。`begin` / `end` / `get` / `lower_bound` の戻り値で使う。区間が存在しないことは `end()` で表す（`not_end` で判定できる）。

## メンバ変数

| 名前 | 型 | 説明 |
|---|---|---|
| `e` | `static Value const` | `f_e()` の値 |
| `st` | `set<node, comp>` | 区間を保持する set。左端だけで順序付けられている |

`st` は読み取り専用として扱うこと（直接書き換えると、同じ値の区間の連結や `add` / `del` との整合が崩れる）。

## コンストラクタ

```cpp
interval_set<T, Value, add, del, f_e> obj()
interval_set<T, Value, add, del, f_e> obj(vector<Value> const& v)
interval_set<T, Value, add, del, f_e> obj(r1, r2, op, bool unpush_e = false)
```

- `obj()` : 空の状態で初期化する。
- `obj(v)` : 各 $i$ について、点 $i$（区間 $[i, i + 1)$）の値を `v[i]` とした状態で初期化する。隣り合う同じ値は 1 つの区間につながる。できた区間ごとに `add` が呼ばれる。
- `obj(r1, r2, op, unpush_e)` : 座標の型 `T` が共通な 2 つの interval_set `r1`, `r2` を点ごとに `op` で合成する。
  - `r1`, `r2` の少なくとも一方の区間に含まれる点 $x$ の値を、`op(r1 での値, r2 での値)` とする。区間外の側は `r1.e` / `r2.e` を使う
  - どちらにも含まれない点は区間外のままにする
  - `unpush_e = true` なら、合成した値が $e$ になる点も区間外にする
  - できた区間ごとに `add` が呼ばれる（`r1`, `r2` のコールバックは呼ばれない）
  - `r1`, `r2` は値の型やコールバックが `obj` と違ってもよい

**制約**

- `obj(v)` : `T` が `int` から構築できること
- `obj(r1, r2, op, unpush_e)` : `op` が `Value(V1, V2)` として呼び出せること（`V1`, `V2` は `r1`, `r2` の値の型）

**計算量**

- `obj()` : $O(1)$
- `obj(v)` : $O(|v|)$
- `obj(r1, r2, op, unpush_e)` : `r1`, `r2` の区間数を $n_1, n_2$ として $O(n_1 + n_2)$

**使用例**

```cpp
interval_set<int, int> s;
interval_set<int, char> t(vector<char>{'a', 'a', 'b', 'b', 'b', 'a'});
// t: [0,2):a [2,5):b [5,6):a

interval_set<int, int> a, b;
a.update(0, 6, 1);
b.update(4, 10, 2);
interval_set<int, int> c(a, b, [](int x, int y) { return x + y; });
// c: [0,4):1 [4,6):3 [6,10):2
interval_set<int, int> d(a, b, [](int x, int y) { return x * y; }, true);
// d: [4,6):2  (値が e = 0 になる [0,4) と [6,10) は入れない)
```

## コピー・ムーブ・代入・破棄

```cpp
interval_set<T, Value, add, del, f_e> b(a)       // コピー
interval_set<T, Value, add, del, f_e> b(move(a)) // ムーブ
b = a                                            // コピー代入
b = move(a)                                      // ムーブ代入
```

区間が interval_set に入る・出るたびに `add` / `del` が呼ばれる。

| 操作 | `add` | `del` |
|---|---|---|
| コピー `b(a)` | `b` に入る区間ごと | — |
| ムーブ `b(move(a))` | — | — |
| コピー代入 `b = a` | `b` に入る区間ごと | `b` の元の区間ごと |
| ムーブ代入 `b = move(a)` | — | `b` の元の区間ごと |
| 破棄 | — | 残っている区間ごと |

ムーブは区間をインスタンス間で移すだけなので、`add` / `del` を呼ばない。移動元の `a` は空になる。

**計算量**

- コピー・コピー代入・破棄: 区間数を $n$ として $O(n)$（`add` / `del` の呼び出しを含む）
- ムーブ: $O(1)$
- ムーブ代入: `b` の元の区間数を $n$ として $O(n)$

**使用例**

```cpp
long long len = 0; // 覆われている長さの合計 (interval_set より前に定義する)
void add_len(int l, int r, int) { len += r - l; }
void del_len(int l, int r, int) { len -= r - l; }
using IS = interval_set<int, int, add_len, del_len>;

{
  IS a;
  a.update(0, 5); // len == 5
  IS b(a);        // len == 10 (b の区間も数える)
  b.update(5, 8); // len == 13 (b: [0,8))
  b = a;          // len == 10 (b の元の [0,8) を del, a のコピー [0,5) を add)
  IS c(move(b));  // len == 10 (移すだけ. b は空になる)
}                 // len == 0  (a, b, c の破棄で del)
```

## 更新系

### update

```cpp
void obj.update(T l, T r, Value v = e)
void obj.update(T l, T r, Args&&... args)
```

$[l, r)$ を値 `v` で上書きする。`v` を省略すると値 $e$ で上書きする（区間集合として使うときの挿入）。$[l, r)$ と重なっていた区間は分割・削除され、接している同じ値の区間とは連結される。$l \geq r$ のときは何もしない。

2 つ目の形は、`Value(args...)` で構築した値で上書きする。`explicit` なコンストラクタや複数の引数から構築するときに使われる。引数が `Value` 型の値や波括弧の初期化子のときは 1 つ目の形が使われる。

**制約**

- 2 つ目の形: `constructible_from<Value, Args...>` を満たすこと

**計算量**

- 償却 $O(\log n)$（削除される区間数を $k$ として $O((k + 1) \log n)$）＋ `add` / `del` の呼び出しコスト

**使用例**

```cpp
interval_set<int, int> s;
s.update(0, 10, 1);   // [0,10):1
s.update(3, 5, 2);    // [0,3):1 [3,5):2 [5,10):1
s.update(5, 7, 2);    // [0,3):1 [3,7):2 [7,10):1  ([3,5):2 とつながる)
s.update(3, 4, s[0]); // [0,4):1 [4,7):2 [7,10):1  (点 0 の値で塗る)

interval_set<int, string> t;
t.update(0, 3, 3, 'a');            // [0,3):"aaa"  (string(3, 'a') で構築)
t.update(3, 5, string_view("bc")); // [3,5):"bc"   (explicit な変換)

interval_set<int, pair<int, int>> p;
p.update(0, 5, {1, 2}); // [0,5):(1,2)  (波括弧の初期化子)
p.update(5, 8, 3, 4);   // [5,8):(3,4)  (pair(3, 4) で構築)
```

---

### erase

```cpp
void obj.erase(T l, T r)
```

$[l, r)$ にかかる部分を削除する。区間のうち $[l, r)$ の外側の部分は残る。$l \geq r$ のときは何もしない。

**計算量**

- 償却 $O(\log n)$（削除される区間数を $k$ として $O((k + 1) \log n)$）＋ `add` / `del` の呼び出しコスト

**使用例**

```cpp
// s: [0,4):1 [4,7):2 [7,10):1
s.erase(8, 12);        // [0,4):1 [4,7):2 [7,8):1
auto it = s.get(5);
s.erase(it->l, it->r); // 点 5 を含む区間を丸ごと削除: [0,4):1 [7,8):1
```

## 取得系

### get

```cpp
Itr obj.get(T const& p)
```

点 $p$ を含む区間のイテレータを返す。存在しなければ `end()` を返す。

**計算量**

- $O(\log n)$

**使用例**

```cpp
interval_set<int, int> s;
s.update(0, 3, 1);
s.update(5, 8, 2);
auto it = s.get(6);
if(s.not_end(it)) cout << it->l << " " << it->r << " " << it->v << endl; // 5 8 2
s.not_end(s.get(4)); // false
```

---

### lower_bound

```cpp
Itr obj.lower_bound(T const& p)
```

$p$ 以上の点を含む区間のうち最も左にあるもの（$r > p$ となる最初の区間）のイテレータを返す。存在しなければ `end()` を返す。

**計算量**

- $O(\log n)$

**使用例**

```cpp
// s: [0,3):1 [5,8):2
s.lower_bound(2)->l;         // 0 ([0,3) が 2 を含む)
s.lower_bound(3)->l;         // 5 (3 を含む区間はないので次の区間)
s.not_end(s.lower_bound(8)); // false
```

---

### covered

```cpp
bool obj.covered(T const& p)
bool obj.covered(T const& l, T const& r)
```

- `covered(p)` : 点 $p$ を含む区間が存在するかを返す。
- `covered(l, r)` : $[l, r)$ 全体を含む区間が 1 つ存在するかを返す。$l = r$ のときは true。

値の異なる区間にまたがる場合は、すき間なく覆われていても false になる。区間集合として使う場合は「$[l, r)$ がすべて覆われているか」と同じ。

**制約**

- $l \leq r$（違反時は `assert`）

**計算量**

- $O(\log n)$

**使用例**

```cpp
interval_set<int, int> s;
s.update(0, 5, 1);
s.update(5, 10, 2);
s.covered(3);    // true
s.covered(3, 5); // true
s.covered(3, 7); // false ([0,5):1 と [5,10):2 にまたがる)
s.covered(7, 7); // true
```

---

### same

```cpp
bool obj.same(T const& p, T const& q)
```

$p$ と $q$ が同じ区間に含まれるかを返す。どちらかが区間外なら false。

**計算量**

- $O(\log n)$

**使用例**

```cpp
// s: [0,3):1 [5,8):2
s.same(0, 2); // true
s.same(2, 5); // false
s.same(3, 3); // false (3 は区間外)
```

---

### get_val / operator[]

```cpp
Value const& obj.get_val(T const& p)
Value const& obj[p]
```

点 $p$ を含む区間の値を返す。存在しなければ $e$ を返す。

戻り値は集合内の値（または $e$）への参照で、以降の `update` / `erase` で無効になることがある。値が $e$ の区間と区間外は区別できないので、必要なら `covered` を使う。

**計算量**

- $O(\log n)$

**使用例**

```cpp
interval_set<int, int> s;
s.update(0, 3, 0);
s[1];         // 0 (値 0 の区間)
s[5];         // 0 (区間外なので e = 0)
s.covered(1); // true
s.covered(5); // false
```

---

### get_mex

```cpp
T obj.get_mex(T const& p = 0)
```

$p$ 以上で、どの区間にも含まれない最小の点を返す。

**制約**

- 引数を省略する場合は、`T` が `0` から構築できること

**計算量**

- $p$ を含む区間から右に接して続く区間の数を $k$ として $O(\log n + k)$
- 区間集合として使う場合（値がすべて同じ）は、接する区間がすべて連結されているので $O(\log n)$

**使用例**

```cpp
interval_set<long long> s;
s.update(1, 3);
s.update(3, 6); // [1,6) にまとまる
s.update(8, 9);
s.get_mex(0); // 0
s.get_mex(1); // 6
s.get_mex(8); // 9

interval_set<int, int> t;
t.update(0, 5, 1);
t.update(5, 10, 2);
t.get_mex(0); // 10 (値の異なる [5,10) もたどる)
```

---

### size

```cpp
size_t obj.size()
```

区間の個数を返す。接している同じ値の区間は 1 個に連結されている。

**計算量**

- $O(1)$

---

### begin / end

```cpp
Itr obj.begin()
Itr obj.end()
```

区間を左端の昇順にたどるイテレータを返す。範囲 for で `node` を列挙できる。

**計算量**

- $O(1)$

**使用例**

```cpp
// s: [0,3):1 [5,8):2
for(auto& [l, r, v]: s) cout << l << " " << r << " " << v << "\n";
// 0 3 1
// 5 8 2
```

---

### not_end

```cpp
bool obj.not_end(Itr it)
```

`it != obj.end()` を返す。`get` / `lower_bound` の戻り値の判定に使う。

**計算量**

- $O(1)$

## 出力

### operator<<

```cpp
ostream& operator<<(ostream& os, interval_set const& obj)
ostream& operator<<(ostream& os, node const& nd)
```

区間を `([l, r): v)` の形式で、左から順に区切りなしで出力する。

**制約**

- `T`, `Value` が `<<` で出力できること

**使用例**

```cpp
// s: [0,3):1 [3,7):2 [7,8):1
cerr << s << endl; // ([0, 3): 1)([3, 7): 2)([7, 8): 1)
```
