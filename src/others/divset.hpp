#pragma once
#include "lion/noop.hpp"
#include <bits/stdc++.h>
using namespace std;

// 比較関数によって決められる大きいものk個とそれ以外を持つ構造体.
template<class T, auto add = noop, auto del = noop, auto comp = less<T>()>
  requires is_invocable_v<decltype(add), T const&> && is_invocable_v<decltype(del), T const&> &&
           strict_weak_order<decltype(comp), T, T>
struct divset {
  multiset<T, decltype(comp)> mn{comp}, mx{comp};
  int k;
  constexpr divset(int k_ = 0): k(k_) {}
  constexpr void fix() {
    assert(k >= 0);
    while(int(mx.size()) > k) {
      auto it = mx.begin();
      del(*it);
      mn.insert(mn.end(), mx.extract(it));
    }
    while(int(mx.size()) < k && !mn.empty()) {
      auto it = prev(mn.end());
      add(*it);
      mx.insert(mx.begin(), mn.extract(it));
    }
  }
  // clang-format off
  constexpr divset& operator++() { ++k; fix(); return *this; }
  constexpr divset& operator--() { --k; fix(); return *this; }
  // clang-format on
  template<class... Args>
    requires constructible_from<T, Args...>
  constexpr void emplace(Args&&... args) {
    T x(forward<Args>(args)...);
    if(int(mx.size()) >= k && (mx.empty() || !comp(*mx.begin(), x))) mn.emplace(move(x));
    else add(x), mx.emplace(move(x)), fix();
  }
  // 削除できたかを返す.
  constexpr bool erase(T const& x) {
    if(!mn.empty() && !comp(*mn.rbegin(), x)) {
      if(auto it = mn.find(x); it != mn.end()) return mn.erase(it), true;
      return false;
    }
    if(auto it = mx.find(x); it != mx.end()) return del(*it), mx.erase(it), fix(), true;
    return false;
  }
};
