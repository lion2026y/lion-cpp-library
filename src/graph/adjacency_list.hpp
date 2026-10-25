#pragma once
#include <bits/stdc++.h>
using namespace std;

// n 頂点 m 辺のグラフを cin から読み込み, 隣接リストを返す. 各辺は "u v" の形式.
// directed: true なら u -> v のみ, false なら両方向に辺を張る.
// one_indexed: true なら入力の頂点番号を -1 して 0-indexed にする.
vector<vector<int>> adjacency_list(int n, int m, bool directed = false, bool one_indexed = true) {
  vector<array<int, 2>> es(m);
  vector<int> deg(n);
  for(auto& [u, v]: es) {
    cin >> u >> v;
    if(one_indexed) --u, --v;
    assert(0 <= u && u < n && 0 <= v && v < n);
    ++deg[u];
    if(!directed) ++deg[v];
  }
  // 次数ぶん先に確保して, 辺ごとの再確保を避ける.
  vector<vector<int>> to(n);
  for(int i = 0; i < n; ++i) to[i].reserve(deg[i]);
  for(auto [u, v]: es) {
    to[u].emplace_back(v);
    if(!directed) to[v].emplace_back(u);
  }
  return to;
}
// n 頂点の木（n - 1 辺, 無向, 1-indexed）を読み込む.
vector<vector<int>> adjacency_list(int n) { return adjacency_list(n, max(n - 1, 0)); }
// 重み付き版. 各辺は "u v c" の形式で, to[u] に {v, c} を持つ.
template<class T>
vector<vector<pair<int, T>>> adjacency_list_cost(int n, int m, bool directed = false, bool one_indexed = true) {
  vector<tuple<int, int, T>> es(m);
  vector<int> deg(n);
  for(auto& [u, v, c]: es) {
    cin >> u >> v >> c;
    if(one_indexed) --u, --v;
    assert(0 <= u && u < n && 0 <= v && v < n);
    ++deg[u];
    if(!directed) ++deg[v];
  }
  vector<vector<pair<int, T>>> to(n);
  for(int i = 0; i < n; ++i) to[i].reserve(deg[i]);
  for(auto& [u, v, c]: es) {
    to[u].emplace_back(v, c);
    if(!directed) to[v].emplace_back(u, c);
  }
  return to;
}
// n 頂点の重み付き木（n - 1 辺, 無向, 1-indexed）を読み込む.
template<class T>
vector<vector<pair<int, T>>> adjacency_list_cost(int n) { return adjacency_list_cost<T>(n, max(n - 1, 0)); }
