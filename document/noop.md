# noop

[Source](../src/utils/noop.hpp)

## 概要

任意個・任意型の引数を受け取って何もしない関数オブジェクト。

コールバックを受け取るライブラリのデフォルト引数として使う（例: [divset](divset.md) の `add` / `del`）。キャプチャなしのラムダなので、テンプレート引数（NTTP）としてもそのまま渡せる。

## 定義

```cpp
inline constexpr auto noop = [](auto&&...) {};
```

**計算量**

- $O(1)$（インライン化されるので実行コストはない）

**使用例**

```cpp
noop();
noop(1, "a", 3.0);

template<class T, auto f = noop> struct S { void g(T const& x) { f(x); } };
S<int> s;
s.g(3); // 何もしない
```
