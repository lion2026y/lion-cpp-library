#pragma once
#include <bits/stdc++.h>
using namespace std;

// 分数.
// T2は比較時に変換される型.
template<class T = long long, class T2 = T>
struct frac {
  T n, d; // c=分子,d=分母.
  constexpr frac(): n(0), d(1) {}
  template<integral I = T, integral J = I>
  constexpr frac(I const& _n = 0, J const& _d = 1, bool b = true): n(_n), d(_d) {
    if(b) irr();
  }
  // 約分.
  constexpr frac& irr() {
    T g = gcd(n, d);
    n /= g, d /= g;
    if(d < 0) n *= -1, d *= -1;
    return *this;
  }

  constexpr frac& operator+=(frac const& ot) {
    T g = gcd(d, ot.d);
    n = ot.d / g * n + d / g * ot.n, d *= ot.d / g;
    return irr();
  }
  constexpr frac& operator-=(frac const& ot) { return operator+=(frac(ot.n * -1, ot.d, false)); }
  constexpr frac& operator*=(frac const& ot) {
    T g1 = gcd(n, ot.d), g2 = gcd(d, ot.n);
    n /= g1, n *= ot.n / g2;
    d /= g2, d *= ot.d / g1;
    return *this;
  }
  constexpr frac& operator/=(frac const& ot) { return operator*=(frac(ot.d, ot.n)); }
  constexpr frac operator+(frac const& ot) const { return frac(*this) += ot; }
  constexpr frac operator-(frac const& ot) const { return frac(*this) -= ot; }
  constexpr frac operator*(frac const& ot) const { return frac(*this) *= ot; }
  constexpr frac operator/(frac const& ot) const { return frac(*this) /= ot; }
  // clang-format off
  template<integral I> constexpr frac& operator+=(I const& ot) { return operator+=(frac(ot)); }
  template<integral I> constexpr frac& operator-=(I const& ot) { return operator-=(frac(ot)); }
  template<integral I> constexpr frac& operator*=(I const& ot) { return operator*=(frac(ot)); }
  template<integral I> constexpr frac& operator/=(I const& ot) { return operator/=(frac(ot)); }
  template<integral I> constexpr frac operator+(I const& ot) const { return operator+(frac(ot)); }
  template<integral I> constexpr frac operator-(I const& ot) const { return operator-(frac(ot)); }
  template<integral I> constexpr frac operator*(I const& ot) const { return operator*(frac(ot)); }
  template<integral I> constexpr frac operator/(I const& ot) const { return operator/(frac(ot)); }
  template<integral I> friend constexpr frac operator+(I const& ot, frac const& f) { return frac(ot) + f; }
  template<integral I> friend constexpr frac operator-(I const& ot, frac const& f) { return frac(ot) - f; }
  template<integral I> friend constexpr frac operator*(I const& ot, frac const& f) { return frac(ot) * f; }
  template<integral I> friend constexpr frac operator/(I const& ot, frac const& f) { return frac(ot) / f; }
  // clang-format on
  constexpr bool operator==(frac const& o) const { return T2(n) * o.d == T2(d) * o.n; }
  constexpr auto operator<=>(frac const& o) const { return T2(n) * o.d <=> T2(d) * o.n; }

  // clang-format off
  friend istream& operator>>(istream& i, frac<T, T2>& f) { i >> f.n >> f.d; f.irr(); return i; }
  friend ostream& operator<<(ostream& o, frac<T, T2> const& f) { return o << format("{}/{}", f.n, f.d); }
  // clang-format on
};
