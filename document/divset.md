# divset

[Source](../src/others/divset.hpp)

## 概要

多重集合を、比較関数 `comp` の意味で **大きいもの $k$ 個**（`mx`）と **それ以外**（`mn`）に分けて保持する構造体。要素の追加・削除や $k$ の増減をしても、この分割が常に保たれる。

要素が `mx` に入るときに `add`、`mx` から出るときに `del` が呼ばれる。これを使うと「上位 $k$ 個の和」などの集計値を、差分更新だけで管理できる。

常に次の性質が成り立つ。

- $|\mathrm{mx}| = \min(k, \text{全要素数})$
- `mn` の任意の要素 $a$ と `mx` の任意の要素 $b$ について $\lnot\,\mathrm{comp}(b, a)$（すなわち $a \leq b$）

注意点

- `add` / `del` / `comp` はテンプレート引数（NTTP）なので、キャプチャなしのラムダ、関数ポインタ、`less<T>{}` などの構造的型でなければならない。集計値はグローバル変数などに持たせる。
- `comp = greater<T>{}` にすると「小さいもの $k$ 個」を管理できる。テンプレート引数に `greater<T>()` と書くと関数型と解釈されてコンパイルエラーになるので、`{}` を使うこと。
- `comp` が全順序でない場合（キーの一部だけで比較する場合など）、`erase` は $x$ と同値な要素のどれか 1 つを削除する（`std::multiset::find` と同じ挙動）。

## テンプレートパラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `T` | — | 要素の型 |
| `add` | `noop` | 要素が `mx` に入るときに呼ばれる。`add(T const&)` として呼び出せること |
| `del` | `noop` | 要素が `mx` から出るときに呼ばれる。`del(T const&)` として呼び出せること |
| `comp` | `less<T>{}` | 比較関数。`strict_weak_order<decltype(comp), T, T>` を満たすこと |

`noop` は [noop.hpp](noop.md) で定義される「何もしない関数オブジェクト」。

## メンバ変数

| フィールド | 型 | 説明 |
|---|---|---|
| `mx` | `multiset<T, decltype(comp)>` | 大きいもの $k$ 個 |
| `mn` | `multiset<T, decltype(comp)>` | それ以外の要素 |
| `k` | `int` | `mx` に保持する個数 |

`mx` / `mn` は読み取り専用として扱うこと（直接書き換えると `add` / `del` との整合が崩れる）。

## コンストラクタ

```cpp
divset<T, add, del, comp> obj(int k = 0)
```

- `obj(k)` : 空の状態で、上位 $k$ 個を管理するように初期化する。

**制約**

- $k \geq 0$

**計算量**

- $O(1)$

**使用例**

```cpp
long long s = 0;
divset<int, [](int x) { s += x; }, [](int x) { s -= x; }> d(2);
divset<int, noop, noop, greater<int>{}> small(3); // 小さいもの 3 個
```

## 関数

### emplace

```cpp
void obj.emplace(Args&&... args)
```

`T(args...)` で構築した要素を追加する。要素が `mx` に入る場合は `add` が呼ばれ、追い出された要素に対して `del` が呼ばれる。

**制約**

- `constructible_from<T, Args...>`

**計算量**

- $O(\log n)$

**使用例**

```cpp
long long s = 0;
divset<int, [](int x) { s += x; }, [](int x) { s -= x; }> d(2);
d.emplace(5);
d.emplace(1);
d.emplace(9);
d.emplace(3);
// s == 14 (9 + 5)
```

---

### erase

```cpp
bool obj.erase(T const& x)
```

$x$ と同値な要素を 1 つ削除し、削除できたかを返す。`mx` から削除した場合は `del` が呼ばれ、`mn` の最大要素が `mx` に補充されて `add` が呼ばれる。

`mn` と `mx` の両方に同値な要素がある場合は `mn` 側から削除するため、コールバックは呼ばれない。

**計算量**

- $O(\log n)$

**使用例**

```cpp
d.erase(9);         // true, s == 8 (5 + 3)
bool ok = d.erase(100); // false
assert(!ok);        // assert(d.erase(x)) と書くと NDEBUG 時に erase ごと消えるので注意
```

---

### operator++ / operator--

```cpp
divset& ++obj
divset& --obj
```

$k$ を 1 増やす / 減らし、分割を更新する。

**制約**

- `--obj` の後も $k \geq 0$ であること（違反時は `assert`）

**計算量**

- $O(\log n)$

**使用例**

```cpp
++d; // k = 3, s == 9 (5 + 3 + 1)
--d; // k = 2, s == 8 (5 + 3)
```

---

### fix

```cpp
void obj.fix()
```

$|\mathrm{mx}| = \min(k, \text{全要素数})$ となるように `mx` と `mn` の間で要素を移動する。`k` を直接書き換えた後に呼ぶことで、$k$ を一度に大きく変更できる。

**制約**

- $k \geq 0$（違反時は `assert`）

**計算量**

- 移動する要素数を $d$ として、$O(d)$ 償却（ノードの付け替えとヒント付き挿入を使うため）＋コールバックのコスト

**使用例**

```cpp
d.k = 4;
d.fix(); // s == 9 (5 + 3 + 1, 要素が 3 個しかないため)
```
