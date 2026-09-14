# sieve

[Source](../src/algebra/sieve.hpp)

## 概要

エラトステネスの篩により、$[0, n]$ の範囲の**最小素因数テーブル**と**素数リスト**を前計算する構造体。素数判定・素因数分解・約数列挙を提供する。

- 最小素因数を持つため、$x < n$ なら素数判定が $O(1)$、素因数分解が $O(\log x)$ で行える。
- $x \geq n$ の場合も、素数リストによる試し割りで $x < n^2$ の範囲まで対応する。つまり $n = 10^7$ 程度に取れば $10^{14}$ 程度までの値を扱える。
- 構築時に 2 の倍数をまとめて埋め、以降は奇数のみを走査することで定数倍を削減している。
- `begin` / `end` を持つため、素数リストを `for(int p : s)` の形で直接走査できる。

注意点：

- メモリを $4n$ バイト消費する。$n = 10^7$ なら約 40 MB、構築に 100〜150 ms 程度かかるため、実行時間・メモリ制限に対して $n$ を過大に取らないこと。
- `factor` / `factors` / `divisors` は $x > 0$ を要求する（$x = 0$ で無限ループにならないよう `assert` で弾いている）。負数の素因数分解はサポートしない。
- `factor` / `factors` は $x < n^2$ を要求する。この範囲を超えると $\sqrt{x}$ 以下の素数がテーブルに揃わず、素因数分解が正しくならないため `assert` で弾く。
- `divisors` のみ $x \geq n^2$ でも $O(\sqrt{x})$ の試し割りにフォールバックするため、範囲制限なしに使える。
- `isprime` は $x \leq 0$ に対して `false` を返す。

## メンバ変数

| フィールド | 型 | 説明 |
|---|---|---|
| `n` | `long long` | テーブルサイズ。コンストラクタ引数 `_n` に対し `max(_n, 1) + 1` となり、添字 $0$ 以上 $n$ 未満（すなわち $0$ 以上 `_n` 以下）を扱える |
| `pv` | `vector<int>` | 最小素因数テーブル。`pv[i]` は $i$ の最小素因数（$i$ が素数なら `pv[i] == i`）。`pv[0] == pv[1] == -1` |
| `pr` | `vector<int>` | $n$ 未満の素数を昇順に格納したリスト |

## コンストラクタ

```cpp
sieve s(int _n = 1)
```

$0$ 以上 `_n` 以下の整数について最小素因数テーブルを構築する。`_n` が $1$ 未満の場合は $1$ として扱われる（`n == 2`）。

**制約**

- なし（`_n` が $0$ や負でも安全に $1$ へ丸められる）

**計算量**

- $O(n \log \log n)$ 時間、$4n$ バイトのメモリ

**使用例**

```cpp
sieve s(10000000);   // 0 以上 1e7 以下。x < (1e7+1)^2 ≈ 1e14 まで扱える
sieve t(1000);       // 小さく取れば構築も一瞬
```

---

## 関数

### size

```cpp
int s.size()
```

前計算した素数の個数（`pr.size()`）を返す。

**計算量**

- $O(1)$

**使用例**

```cpp
sieve s(10000000);
printf("%d\n", s.size());   // 664579
```

---

### begin / end

```cpp
auto s.begin()
auto s.end()
```

素数リスト `pr` の先頭・終端イテレータを返す。`sieve` 自体を range-based for で走査できる。

**計算量**

- $O(1)$

**使用例**

```cpp
sieve s(30);
for(int p : s) printf("%d ", p);   // 2 3 5 7 11 13 17 19 23 29
```

---

### isprime

```cpp
template<integral I> bool s.isprime(I x)
```

`x` が素数かどうかを判定する。$x < n$ なら最小素因数テーブルの参照のみ、それ以上なら素数リストによる試し割りを行う。

**制約**

- $x < n^2$

**計算量**

- $x \leq 0$ のとき $O(1)$（常に `false`）
- $x < n$ のとき $O(1)$
- $n \leq x < n^2$ のとき $O(\pi(\sqrt{x}))$（$\pi$ は素数計数関数）

**使用例**

```cpp
sieve s(1000);
s.isprime(997);         // true  (x < n なのでテーブル参照)
s.isprime(999983);      // true  (x >= n なので試し割り)
s.isprime(1);           // false
s.isprime(-7);          // false
```

---

### factor

```cpp
template<integral I> vector<array<I, 2>> s.factor(I x)
```

`x` を素因数分解し、`{素因数, 指数}` の配列を素因数の昇順で返す。$x = 1$ のときは空の配列を返す。

**制約**

- $0 < x < n^2$

**計算量**

- $x < n$ のとき $O(\log x)$
- $n \leq x < n^2$ のとき $O(\pi(\sqrt{x}))$

**使用例**

```cpp
sieve s(1000);
for(auto [p, c] : s.factor(360)) printf("%d^%d ", p, c);   // 2^3 3^2 5^1
s.factor(1);                                               // {} (空)
```

---

### factors

```cpp
template<integral I> vector<I> s.factors(I x)
```

`x` の素因数を重複を含めて昇順に列挙する。`factor` と異なり指数をまとめず、素因数を並べた配列を返す。$x = 1$ のときは空の配列を返す。

**制約**

- $0 < x < n^2$

**計算量**

- $x < n$ のとき $O(\log x)$
- $n \leq x < n^2$ のとき $O(\pi(\sqrt{x}))$

**使用例**

```cpp
sieve s(1000);
for(int v : s.factors(360)) printf("%d ", v);   // 2 2 2 3 3 5
```

---

### divisors

```cpp
template<integral I> vector<I> s.divisors(I x, bool do_sort = true)
```

`x` の約数をすべて列挙する。`do_sort` が `true`（既定）なら昇順にソートして返し、`false` なら順序は不定（生成順）。順序が不要なら `false` を渡すことでソート分の時間を省ける。

$x < n^2$ なら素因数分解を経由して約数を構成するため、$O(\sqrt{x})$ の試し割りより大幅に高速（$n = 10^7$, $x \approx 10^{14}$ で約 40 倍）。$x \geq n^2$ では $O(\sqrt{x})$ の試し割りにフォールバックする。

**制約**

- $0 < x$（`factor` / `factors` と違い $x < n^2$ の制約はない）

**計算量**

$d(x)$ を `x` の約数の個数として、

- $x < n$ のとき $O(\log x + d(x))$
- $n \leq x < n^2$ のとき $O(\pi(\sqrt{x}) + d(x))$
- $x \geq n^2$ のとき $O(\sqrt{x})$
- `do_sort == true` の場合は上記に加えて $O(d(x) \log d(x))$

**使用例**

```cpp
sieve s(1000);
for(int v : s.divisors(36)) printf("%d ", v);      // 1 2 3 4 6 9 12 18 36
auto d = s.divisors(36, false);                     // ソートしない（順序不定）
```
