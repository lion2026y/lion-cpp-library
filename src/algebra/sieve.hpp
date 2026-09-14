#pragma once
#include <bits/stdc++.h>
using namespace std;

// エラトステネスの篩.
// 最小素因数テーブルを持ち, 素数判定/素因数分解/約数列挙ができる.
// 構築 O(n log log n), メモリ 4n バイト.
struct sieve {
  long long n;
  vector<int> pv, pr; // 最小素因数,素数.
  constexpr sieve(int _n = 1): n(max(_n, 1) + 1), pv(n) {
    pv[0] = pv[1] = -1;
    if(n >= 16) pr.reserve(n * 2 / bit_width((unsigned long long)n));
    // 2 の倍数を先に埋め, 以降は奇数のみ走査する(定数倍改善).
    if(n > 2) {
      pr.emplace_back(2);
      pv[2] = 2;
      for(long long j = 4; j < n; j += 2) pv[j] = 2;
    }
    for(long long i = 3; i < n; i += 2)
      if(!pv[i]) {
        pr.emplace_back(i);
        pv[i] = i;
        for(long long j = i * i; j < n; j += 2 * i)
          if(!pv[j]) pv[j] = i;
      }
  }
  constexpr int size() const { return pr.size(); }
  constexpr auto begin() const { return pr.begin(); }
  constexpr auto end() const { return pr.end(); }
  // x<n?O(1):O(√x)
  constexpr bool isprime(integral auto x) const {
    if(x < 2) return false;
    if(x < n) return pv[x] == x;
    assert(x < n * n);
    for(int p: pr) {
      if(1LL * p * p > x) return true;
      if(!(x % p)) return false;
    }
    return true;
  }
  // x<n?O(logx):O(√x 以下の素数の個数)
  // 素因数分解({素因数,指数}を返す)
  template<integral I>
  constexpr vector<array<I, 2>> factor(I x) const {
    assert(0 < x);
    vector<array<I, 2>> r;
    if(x < n) {
      while(x != 1) {
        if(!r.empty() && r.back()[0] == I(pv[x])) ++r.back()[1];
        else r.emplace_back(array<I, 2>{I(pv[x]), 1});
        x /= pv[x];
      }
    } else {
      assert(x < n * n);
      for(int p: pr) {
        if(1LL * p * p > x) break;
        if(!(x % p)) {
          r.emplace_back(array<I, 2>{I(p), 1});
          while(!((x /= p) % p)) ++r.back()[1];
        }
        if(x == 1) break;
      }
      if(x != 1) r.emplace_back(array<I, 2>{x, 1});
    }
    return r;
  }
  // x<n?O(logx):O(√x 以下の素数の個数)
  // 素因数列挙
  template<integral I>
  constexpr vector<I> factors(I x) const {
    assert(0 < x);
    vector<I> r;
    if(x < n) {
      while(x != 1) r.emplace_back(pv[x]), x /= pv[x];
    } else {
      assert(x < n * n);
      for(int p: pr) {
        if(1LL * p * p > x) break;
        while(!(x % p)) r.emplace_back(p), x /= p;
        if(x == 1) break;
      }
      if(x != 1) r.emplace_back(x);
    }
    return r;
  }

  // x<n*n?O(π(√x)+約数の個数):O(√x)
  // 約数列挙(ソートするとその分の時間がかかる)
  template<integral I>
  constexpr vector<I> divisors(I x, bool do_sort = true) const {
    assert(0 < x);
    vector<I> r;
    if(x < n * n) {
      r.emplace_back(1);
      for(auto [p, c]: factor(x)) {
        int s = r.size();
        for(int i = 0; i < s; ++i) {
          I v = r[i];
          for(I j = 0; j < c; ++j) r.emplace_back(v *= p);
        }
      }
    } else {
      for(long long i = 1; i * i <= x; ++i)
        if(!(x % i)) {
          r.emplace_back(i);
          if(i * i != x) r.emplace_back(x / i);
        }
    }
    if(do_sort) ranges::sort(r);
    return r;
  }
};
