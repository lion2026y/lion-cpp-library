# point

[Source](../src/geometry/point.hpp)

## 概要

2次元座標・ベクトルを表す構造体。整数座標（`long long`）と浮動小数点座標（`long double` 等）の両方に対応する。

競プロでの主な用途：座標管理、ベクトル演算（内積・外積・偏角）、各種距離計算。

注意: `unit()` は `T` が浮動小数点型のときのみ使用可能（`requires floating_point<T>`）。

## テンプレートパラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `T` | `long long` | 座標の型。整数型・浮動小数点型どちらも可 |

## コンストラクタ

```cpp
point<T> p()
point<T> p(T1 x, T2 y)
```

- `p()` : $(0, 0)$ で初期化する。
- `p(x, y)` : 座標 $(x, y)$ で初期化する。型 `T1`, `T2` は `T` に変換できれば何でも可。

**計算量**

- $O(1)$

**使用例**

```cpp
point<> p;           // (0, 0)
point<> q(3, 4);     // (3, 4)
point<long double> r(1.5, 2.5);
```

## 関数

### is_zero

```cpp
bool obj.is_zero()
```

点・ベクトルが原点（零ベクトル）かどうかを返す。

**計算量**

- $O(1)$

---

### 算術演算子

```cpp
point obj + ot
point obj - ot
point obj * t       // スカラー倍
point obj / t       // スカラー除算
point -obj          // 符号反転
point& obj += ot
point& obj -= ot
point& obj *= t
point& obj /= t
point& ++obj
point& --obj
```

点・ベクトルの四則演算。`*` と `/` はスカラー `T` との演算。

**計算量**

- $O(1)$

---

### 比較演算子

```cpp
bool obj == ot
auto obj <=> ot    // (x, y) の辞書順
```

`<=>` は `(x, y)` の辞書順比較。`std::sort` 等でそのまま使える。

**計算量**

- $O(1)$

---

### dist_sq

```cpp
T1 obj.dist_sq<T1>(point const& ot)
T1 dist_sq<T1>(point const& a, point const& b)
```

2点間の距離の二乗 $|a - b|^2$ を返す。浮動小数点誤差なく整数で比較したい場合に有用。

**制約**

- `T1` は浮動小数点型（`floating_point<T1>`）

**計算量**

- $O(1)$

**使用例**

```cpp
point<> a(0, 0), b(3, 4);
long double d2 = a.dist_sq(b); // 25.0
```

---

### dist

```cpp
T1 obj.dist<T1>(point const& ot)
T1 dist<T1>(point const& a, point const& b)
```

2点間のユークリッド距離 $|a - b|$ を返す。

**制約**

- `T1` は浮動小数点型（`floating_point<T1>`）、デフォルト `long double`

**計算量**

- $O(1)$

**使用例**

```cpp
point<> a(0, 0), b(3, 4);
long double d = a.dist(b); // 5.0
```

---

### m_dist

```cpp
T obj.m_dist(point const& ot)
T m_dist(point const& a, point const& b)
```

2点間のマンハッタン距離 $|x_a - x_b| + |y_a - y_b|$ を返す。

**計算量**

- $O(1)$

---

### slope

```cpp
T1 obj.slope<T1, INF>(point const& ot)
T1 slope<T1, INF>(point const& a, point const& b)
```

2点を通る直線の傾き $(y_{ot} - y) / (x_{ot} - x)$ を返す。垂直（$x = x_{ot}$）のとき `INF` を返す。

**制約**

- `T1` は浮動小数点型（`floating_point<T1>`）、デフォルト `long double`
- `INF` のデフォルトは `numeric_limits<T1>::max()`

**計算量**

- $O(1)$

---

### move

```cpp
point obj.move(int d, T t = 1)
```

方向 `d` にスカラー `t` だけ移動した点を返す。グリッド問題で使用。

方向は静的メンバ `_dr`/`_dc` で定義され、インデックスと向きの対応は以下のとおり：

| `d` | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| 向き | 左 | 上 | 右 | 下 | 左上 | 右上 | 右下 | 左下 |

**計算量**

- $O(1)$

**使用例**

```cpp
point<> p(2, 3);
point<> q = p.move(2); // (3, 3)（右に1移動）
point<> r = p.move(3, 2); // (2, 5)（下に2移動）
```

## ベクトル演算

### length

```cpp
T1 obj.length<T1>()
```

ベクトルの長さ $\sqrt{x^2 + y^2}$ を返す。

**制約**

- `T1` は浮動小数点型（`floating_point<T1>`）、デフォルト `long double`

**計算量**

- $O(1)$

---

### unit

```cpp
point obj.unit()
```

単位ベクトル（長さ1のベクトル）を返す。

**制約**

- `T` が浮動小数点型（`requires floating_point<T>`）
- $\mathrm{is\_zero()} = \mathrm{false}$

**計算量**

- $O(1)$

---

### dot_product

```cpp
T1 obj.dot_product<T1>(point const& ot)
T1 dot_product<T1>(point const& a, point const& b)
```

内積 $x \cdot x_{ot} + y \cdot y_{ot}$ を返す。

**計算量**

- $O(1)$

**使用例**

```cpp
point<> a(1, 0), b(0, 1);
long long d = dot_product(a, b); // 0（直交）
```

---

### cross_product

```cpp
T1 obj.cross_product<T1>(point const& ot)
T1 cross_product<T1>(point const& a, point const& b)
```

外積（$z$ 成分） $x \cdot y_{ot} - y \cdot x_{ot}$ を返す。負の値も返しうる。符号で左右判定に使える。

**計算量**

- $O(1)$

---

### angle

```cpp
T1 obj.angle<T1>(point const& ot)
T1 angle<T1>(point const& a, point const& b)
```

2つのベクトルのなす角 $\theta \in [0, \pi]$ を返す。

**制約**

- `T1` は浮動小数点型（`floating_point<T1>`）、デフォルト `long double`
- 両ベクトルが零ベクトルでないこと

**計算量**

- $O(1)$

---

### argument

```cpp
T1 obj.argument<T1>()
```

ベクトルの偏角（$x$ 軸正方向からの角度）$\theta \in [0, 2\pi)$ を返す。

**制約**

- `T1` は浮動小数点型（`floating_point<T1>`）、デフォルト `long double`
- 零ベクトルでないこと

**計算量**

- $O(1)$

---

### comp_arg

```cpp
bool comp_arg<T1>(point const& a, point const& b)
```

偏角の昇順比較。`T` が整数型のとき浮動小数点を使わずに偏角を比較する。`std::sort` の比較関数として使用可能。

注意: 引数に直接この関数を渡すと見つからないことがあるため、その場合はラムダで包んで渡すこと。

**制約**

- 両ベクトルが零ベクトルでないこと

**計算量**

- $O(1)$

**使用例**

```cpp
vector<point<>> ps = {{1,0},{0,1},{-1,0}};
sort(ps.begin(), ps.end(), [](auto& a, auto& b){ return comp_arg(a, b); });
```

## グリッドアクセス

2次元グリッド（`vector<vector<...>>`, `vector<string>`, 生2次元配列など、ランダムアクセス可能な入れ子range）を点で添字アクセスするユーティリティ。座標 `p` は `r[p.x][p.y]` に対応する（`x` が外側=行、`y` が内側=列。`move` の `_dr`/`_dc` 規約と一致）。

### at

```cpp
decltype(auto) at(R&& r, point p)
```

グリッド `r` を点 `p` で添字アクセスし、要素への参照を返す。`decltype(auto)` で参照を返すため `at(g, p) = v` のように代入もできる。range concept が保証するランダムアクセスイテレータ経由でアクセスするため、`operator[]` を持たないランダムアクセスrangeにも対応する。

**制約**

- `R` がランダムアクセスrangeで、その要素型もランダムアクセスrange（`ranges::random_access_range`）
- `T` が整数型（`integral<T>`）
- `p` はグリッド内の有効な座標であること（**境界チェックなし**。範囲外や負値はUB/RE）

**計算量**

- $O(1)$

**使用例**

```cpp
vector<vector<int>> g(3, vector<int>(4, 0));
at(g, point<>(1, 2)) = 7;      // g[1][2] = 7
int v = at(g, point<>(1, 2));  // 7

vector<string> s = {"abc", "def"};
char c = at(s, point<>(1, 0)); // 'd'
```

---

### at_safe

```cpp
E at_safe(R&& r, point p, E e = E{})
```

`at` の範囲外セーフ版。座標 `p` がグリッド外（負値・サイズ以上）の場合は既定値 `e` を返す。読み取り専用（参照ではなく**値を返す**ため代入はできない）。`e` のデフォルトは要素型のデフォルト値。

**制約**

- `R` がランダムアクセスrangeで、その要素型もランダムアクセスrange
- `T` が整数型（`integral<T>`）
- `E` は要素型（`ranges::range_value_t<ranges::range_value_t<R>>`）。デフォルトで要素型に一致

**計算量**

- $O(1)$

**使用例**

```cpp
vector<vector<int>> g(3, vector<int>(4, 0));
g[1][2] = 7;
int a = at_safe(g, point<>(1, 2));     // 7
int b = at_safe(g, point<>(-1, 0));    // 0（既定値）
int c = at_safe(g, point<>(5, 0), -1); // -1（範囲外→指定した既定値）
```
