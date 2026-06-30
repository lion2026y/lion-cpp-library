# frac

[Source](../src/algebra/fraction.hpp)

## 概要

有理数（分数）を厳密に扱う構造体。常に既約形かつ分母が正の正規形で保持し、四則演算・比較・入出力をサポートする。

- コンストラクタおよび加減算の後に自動で約分（`irr`）し、分母が負なら符号を分子に移して正規化する。
- 加減算は分母の最小公倍数を用い、乗除算は事前にクロス約分することで中間結果のオーバーフローを抑える。
- 比較演算（`==`, `<=>`）では分子・分母をテンプレートパラメータ `T2` に変換してから乗算するため、`T2` をより広い型にすればオーバーフローを回避できる。
- 整数（`integral`）との混在演算をサポートし、`frac op int` / `int op frac` / `frac op= int` がいずれも利用可能。

注意点：

- 四則演算の途中値（特に乗算・加算）は `T` 型のまま計算されるため、`T` の範囲を超える分子・分母が生じるとオーバーフローする。
- 分母 0 や `frac(0, 0)` を構築すると `irr` 内でゼロ除算（未定義動作）となる。利用側で避けること。

## テンプレートパラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `T` | `long long` | 分子・分母を保持する整数型。四則演算もこの型で行われる |
| `T2` | `T` | 比較演算 (`==`, `<=>`) のときに分子・分母を変換する型。オーバーフロー回避用により広い型（例: `__int128`）を指定できる |

## メンバ変数

| フィールド | 型 | 説明 |
|---|---|---|
| `n` | `T` | 分子 |
| `d` | `T` | 分母（正規形では常に正） |

## コンストラクタ

```cpp
frac<T, T2> f()
template<integral I> frac<T, T2> f(I n, I d = 1, bool b = true)
```

- `f()` : `0/1` で初期化する。
- `f(n, d, b)` : 分子 `n`、分母 `d` で初期化する。`b == true`（既定）なら約分・符号正規化を行う。`b == false` のときは正規化を行わず、内部演算で約分を省略したい場合に用いる。

**計算量**

- $O(\log \min(|n|, |d|))$（約分の `gcd` のコスト）

**使用例**

```cpp
frac<long long> a;            // 0/1
frac<long long> b(1, 2);      // 1/2
frac<long long, __int128> c(3, -6); // -1/2（約分・符号正規化される）
```

---

## 関数

### irr

```cpp
frac& f.irr()
```

約分し、分母が負なら符号を分子へ移して正規形にする。自身への参照を返す。

**計算量**

- $O(\log \min(|n|, |d|))$

**使用例**

```cpp
frac<long long> a(2, -4, false); // 正規化を抑えて構築
a.irr();                         // -1/2 になる
```

---

### 四則演算

```cpp
frac  operator+(frac), operator-(frac), operator*(frac), operator/(frac)
frac& operator+=(frac), operator-=(frac), operator*=(frac), operator/=(frac)
// 整数との混在（I は integral）
frac  operator+(I),   operator-(I),   operator*(I),   operator/(I)
frac& operator+=(I),  operator-=(I),  operator*=(I),  operator/=(I)
friend frac operator+(I, frac), operator-(I, frac), operator*(I, frac), operator/(I, frac)
```

有理数同士、および有理数と整数の四則演算を行う。結果は常に既約・分母正の正規形になる。

**制約**

- 除算では右辺が $0$ でないこと。
- 演算途中の分子・分母が `T` の表現範囲を超えないこと。

**計算量**

- $O(\log V)$（`gcd` のコスト。$V$ は関与する値の大きさ）

**使用例**

```cpp
frac<long long> a(1, 2), b(1, 3);
auto s = a + b;   // 5/6
auto p = a * b;   // 1/6
auto q = a / b;   // 3/2
auto r = 5 + a;   // 11/2
a += 3;           // 7/2
```

---

### 比較演算

```cpp
bool f.operator==(frac)
auto f.operator<=>(frac)
```

二つの有理数の大小・等価を比較する。`T2(n) * o.d` と `T2(d) * o.n` を比較するため、`T2` を広い型にすればオーバーフローを避けられる。`<=>` により `<`, `<=`, `>`, `>=` も利用可能。

**計算量**

- $O(1)$

**使用例**

```cpp
frac<long long, __int128> a(1, 2), b(2, 3);
bool e = (a == b);   // false
bool lt = (a < b);   // true
```

---

### 入出力

```cpp
friend istream& operator>>(istream&, frac&)
friend ostream& operator<<(ostream&, const frac&)
```

- 入力：分子・分母をこの順に読み取り、自動で約分する。
- 出力：`分子/分母` の形式で出力する。

**使用例**

```cpp
frac<long long> a;
cin >> a;          // "3 6" を入力すると 1/2 になる
cout << a << "\n"; // "1/2"
```
