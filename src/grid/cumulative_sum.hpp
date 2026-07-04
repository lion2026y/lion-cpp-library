#pragma once
#include <bits/stdc++.h>
using namespace std;

// コンテナのネスト深さ（＝次元数）. スカラーは 0, vector<vector<int>> は 2 など.
template<class T, class = void>
struct nest_depth : integral_constant<int, 0> {};
template<class T>
struct nest_depth<T, void_t<typename T::value_type>>
  : integral_constant<int, nest_depth<typename T::value_type>::value + 1> {};
template<class T>
constexpr int nest_depth_v = nest_depth<T>::value;

// 同形の部分配列 vi, vj の各要素に vi = op(vi, vj) を適用する（in-place）.
template<class V, class F>
constexpr void element_apply(V& vi, V const& vj, F const& op) {
  if constexpr(nest_depth_v<V> == 0) vi = op(vi, vj);
  else {
    assert(vi.size() == vj.size()); // 非矩形（ジャグ配列）を検出.
    for(int i = 0; i < int(vi.size()); ++i) element_apply(vi[i], vj[i], op);
  }
}

// d 次元累積和（in-place）. op は結合的な二項演算（default: 加算）.
// 各要素は「その添字以下（各次元で <= i）」の総和になる.
// 例: cumulative_sum(v);
//     cumulative_sum(v, [](auto a, auto b){ return max(a, b); }); // 2次元 max 累積
template<class V, class F = plus<>>
constexpr V& cumulative_sum(V& v, F const& op = F{}) {
  if constexpr(nest_depth_v<V> == 0) return v;
  else {
    int sz = v.size();
    for(int i = 1; i < sz; ++i) element_apply(v[i], v[i - 1], op);
    for(int i = 0; i < sz; ++i) cumulative_sum(v[i], op);
    return v;
  }
}
