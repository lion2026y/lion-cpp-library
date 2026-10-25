# adjacency_list

[Source](../src/graph/adjacency_list.hpp)

## 概要

標準入力（`cin`）から $n$ 頂点 $m$ 辺のグラフの辺を読み込み、隣接リストを返す関数群。重み付きグラフ用の `adjacency_list_cost` と、頂点数だけを渡して木（$n - 1$ 辺）を読むオーバーロードもある。

競プロで頻出の「`N M` の後に `u_i v_i` が $M$ 行」という入力を、そのまま 0-indexed の隣接リストにする用途を想定している。既定は無向・1-indexed の入力で、第 3 引数 `directed` で有向グラフ、第 4 引数 `one_indexed` で 0-indexed の入力にも対応する。

**注意点**

- 呼び出した時点の `cin` の位置から辺を読むので、`n`, `m` などを読んだ直後に呼ぶ。
- 各 `to[u]` の並びは入力の辺の順になる。多重辺は重複したまま入る。
- 無向グラフの自己ループ $(u, u)$ は、`to[u]` に `u` が 2 回入る（次数の定義と一致する）。有向なら 1 回。
- 頂点番号（`one_indexed` なら $1$ 引いた後）が $[0, n)$ の範囲外だと `assert` で落ちる。`one_indexed` の指定ミス（0-indexed の入力を既定値のまま読むなど）もここで検出できる。
- 辺ごとの再確保を避けるため、辺を一度配列に読み込んで次数を数え、各頂点の `vector` を次数ちょうどに `reserve` してから詰める。そのため関数内で一時的に、辺の数に比例する追加メモリ（重みなしで約 $8m$ バイト）を使う。

## テンプレートパラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `T` | — | `adjacency_list_cost` の辺の重みの型。デフォルト構築可能で、`cin >> c` で読み込めること |

## 関数

### adjacency_list

```cpp
vector<vector<int>> adjacency_list(int n, int m, bool directed = false, bool one_indexed = true)
vector<vector<int>> adjacency_list(int n)
```

- `adjacency_list(n, m, directed, one_indexed)` : 辺 `u v` を $m$ 本読み込み、隣接リスト `to` を返す。`to[u]` は `u` から出る辺の行き先の列。`directed` が `false` なら `to[v]` にも `u` を追加する（無向）。`one_indexed` が `true` なら頂点番号から $1$ 引いて 0-indexed にする。
- `adjacency_list(n)` : $n$ 頂点の木を読む。`adjacency_list(n, max(n - 1, 0))`（無向・1-indexed）と同じ。

**制約**

- $n \geq 0$, $m \geq 0$
- 各頂点番号 $u, v$ は、`one_indexed` なら $1 \leq u, v \leq n$、そうでなければ $0 \leq u, v < n$

**計算量**

- $O(n + m)$

**使用例**

```cpp
// 入力: N M と, 続く M 行の u_i v_i（1-indexed, 無向）
int n, m;
cin >> n >> m;
auto to = adjacency_list(n, m);
// 入力が "3(N) 2(M) / 1 2 / 2 3" なら to = {{1}, {0, 2}, {1}}
for(int v: to[1]) cout << v << '\n'; // 0, 2
```

```cpp
// 有向グラフ（u -> v のみ張る）. 入力が "3(N) 2(M) / 1 2 / 2 3" なら dg = {{1}, {2}, {}}
auto dg = adjacency_list(n, m, true);
// 頂点番号が 0-indexed の入力（無向）
auto g0 = adjacency_list(n, m, false, false);
```

```cpp
// 木: N と, 続く N - 1 行の u_i v_i
int n;
cin >> n;
auto tree = adjacency_list(n);
```

---

### adjacency_list_cost

```cpp
vector<vector<pair<int, T>>> adjacency_list_cost<T>(int n, int m, bool directed = false, bool one_indexed = true)
vector<vector<pair<int, T>>> adjacency_list_cost<T>(int n)
```

- `adjacency_list_cost<T>(n, m, directed, one_indexed)` : 辺 `u v c`（`c` は型 `T` の重み）を $m$ 本読み込み、`to[u]` に `{v, c}` を持つ隣接リストを返す。`directed` と `one_indexed` の意味は `adjacency_list` と同じ。
- `adjacency_list_cost<T>(n)` : $n$ 頂点の重み付き木を読む。`adjacency_list_cost<T>(n, max(n - 1, 0))`（無向・1-indexed）と同じ。

**制約**

- $n \geq 0$, $m \geq 0$
- 各頂点番号 $u, v$ は、`one_indexed` なら $1 \leq u, v \leq n$、そうでなければ $0 \leq u, v < n$
- `T` はデフォルト構築可能で、`cin >> c` で読み込めること

**計算量**

- $O(n + m)$

**使用例**

```cpp
// 入力: N M と, 続く M 行の u_i v_i c_i（1-indexed, 無向）
int n, m;
cin >> n >> m;
auto to = adjacency_list_cost<long long>(n, m);
// 入力が "3(N) 2(M) / 1 2 5 / 2 3 7" なら to = {{{1, 5}}, {{0, 5}, {2, 7}}, {{1, 7}}}
for(auto [v, c]: to[1]) cout << v << ' ' << c << '\n'; // "0 5", "2 7"
```

```cpp
// 有向・重み付き（Dijkstra 法の入力など）
auto dg = adjacency_list_cost<long long>(n, m, true);
// 重み付き木: N と, 続く N - 1 行の u_i v_i c_i
auto tree = adjacency_list_cost<int>(n);
```
