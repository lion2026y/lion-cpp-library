#pragma once
#include <bits/stdc++.h>
using namespace std;

// 座標,ベクトル.
template<class T = long long>
struct point {
  // {左,上,右,下,左上,右上,右下,左下}
  static constexpr int _dr[] = {0, -1, 0, 1, -1, -1, 1, 1};
  static constexpr int _dc[] = {-1, 0, 1, 0, -1, 1, 1, -1};

  T x, y;
  constexpr point(): x(0), y(0) {}
  template<class T1, class T2>
  constexpr point(T1&& _x, T2&& _y): x(forward<T1>(_x)), y(forward<T2>(_y)) {}
  friend istream& operator>>(istream& i, point& p) { return i >> p.x >> p.y; }
  friend ostream& operator<<(ostream& o, point const& p) { return o << p.x << ' ' << p.y; }

  constexpr bool is_zero() const { return x == 0 && y == 0; }
  // clang-format off
  constexpr point operator-() const { return point(-x, -y); }
  constexpr point& operator+=(point const& ot) { x += ot.x, y += ot.y; return *this; }
  constexpr point operator+(point const& ot) const { return point(*this) += ot; }
  constexpr point& operator-=(point const& ot) { return operator+=(-ot); }
  constexpr point operator-(point const& ot) const { return point(*this) += (-ot); }
  constexpr point& operator*=(T const& t) { x *= t, y *= t; return *this; }
  constexpr point operator*(T const& t) const { return point(*this) *= t; }
  constexpr point& operator/=(T const& t) { x /= t, y /= t; return *this; }
  constexpr point operator/(T const& t) const { return point(*this) /= t; }
  constexpr bool operator==(point const& ot) const { return x == ot.x && y == ot.y; }
  constexpr auto operator<=>(point const& ot) const { return tie(x, y) <=> tie(ot.x, ot.y); }
  constexpr point& operator++() { ++x, ++y; return *this; }
  constexpr point& operator--() { --x, --y; return *this; }
  // clang-format on
  template<floating_point T1 = long double>
  constexpr T1 dist_sq(point const& ot) const {
    T1 _dx = T1(x) - ot.x, _dy = T1(y) - ot.y;
    return _dx * _dx + _dy * _dy;
  }
  template<floating_point T1 = long double>
  friend constexpr T1 dist_sq(point const& a, point const& b) { return a.dist_sq<T1>(b); }
  template<floating_point T1 = long double>
  constexpr T1 dist(point const& ot) const { return sqrt(dist_sq<T1>(ot)); }
  template<floating_point T1 = long double>
  friend constexpr T1 dist(point const& a, point const& b) { return a.dist<T1>(b); }
  constexpr T m_dist(point const& ot) const { return abs(x - ot.x) + abs(y - ot.y); }
  friend constexpr T m_dist(point const& a, point const& b) { return a.m_dist(b); }
  template<floating_point T1 = long double, T1 INF = numeric_limits<T1>::max()>
  constexpr T1 slope(point const& ot) const {
    if(x == ot.x) return INF;
    else return (T1(y) - ot.y) / (T1(x) - ot.x);
  }
  template<floating_point T1 = long double, T1 INF = numeric_limits<T1>::max()>
  friend constexpr T1 slope(point const& a, point const& b) { return a.slope<T1, INF>(b); }

  constexpr point move(int d, T const& t = 1) const { return point(x + _dr[d] * t, y + _dc[d] * t); }

  // ベクトル演算.
  template<floating_point T1 = long double>
  constexpr T1 length() const { return sqrt(T1(x) * x + T1(y) * y); }
  constexpr point unit() const
    requires floating_point<T>
  {
    assert(!is_zero());
    return (*this) / (length<T>());
  }
  template<class T1 = T>
  constexpr T1 dot_product(point const& ot) const { return T1(x) * ot.x + T1(y) * ot.y; }
  template<class T1 = T>
  friend constexpr T1 dot_product(point const& a, point const& b) { return a.dot_product<T1>(b); }
  template<class T1 = T>
  // 外積(負の値も返しうる)
  constexpr T1 cross_product(point const& ot) const { return T1(x) * ot.y - T1(y) * ot.x; }
  template<class T1 = T>
  friend constexpr T1 cross_product(point const& a, point const& b) { return a.cross_product<T1>(b); }
  // aとbのなす角 [0,pi]
  template<floating_point T1 = long double>
  constexpr T1 angle(point const& ot) const {
    assert(!is_zero() && !ot.is_zero());
    return acos(clamp(dot_product<T1>(ot) / length<T1>() / ot.length<T1>(), T1(-1), T1(1)));
  }
  template<floating_point T1 = long double>
  friend constexpr T1 angle(point const& a, point const& b) { return a.angle<T1>(b); }
  // ベクトルの偏角 [0,2*pi)
  template<floating_point T1 = long double>
  constexpr T1 argument() const {
    assert(!is_zero());
    T1 r = atan2(T1(y), T1(x));
    if(r >= 0) return r;
    else return r + 2 * numbers::pi_v<T1>;
  }

  // 2次元グリッド r を点 p で添字アクセスする. at(g, p) = ... のように使う.
  template<ranges::random_access_range R>
    requires ranges::random_access_range<ranges::range_value_t<R>> && integral<T>
  friend constexpr decltype(auto) at(R&& r, point const p) { return ranges::begin(ranges::begin(r)[p.x])[p.y]; }
  // p がグリッド外なら e(既定は値型のデフォルト値) を返す, 範囲外セーフな読み取り用.
  template<ranges::random_access_range R, class E = ranges::range_value_t<ranges::range_value_t<R>>>
    requires ranges::random_access_range<ranges::range_value_t<R>> && integral<T>
  friend constexpr E at_safe(R&& r, point const p, E const& e = E{}) {
    if(p.x < 0 || p.x >= T(ranges::size(r))) return e;
    auto&& row = ranges::begin(r)[p.x];
    if(p.y < 0 || p.y >= T(ranges::size(row))) return e;
    return ranges::begin(row)[p.y];
  }
};

// Tが整数型の時に浮動小数点数を使用しない偏角の比較.
// 関数オブジェクトなので sort(v.begin(), v.end(), comp_arg) のようにそのまま渡せる.
// 座標値が大きい場合(~10^18)は comp_arg_t<__int128>{} を使ってオーバーフローを防ぐこと.
template<class T1 = void> // 外積の計算に使う型. void なら座標の型を使う.
struct comp_arg_t {
  template<class T>
  constexpr bool operator()(point<T> const& a, point<T> const& b) const {
    assert(!a.is_zero() && !b.is_zero());
    bool ao = a.y == 0 ? a.x < 0 : a.y < 0, bo = b.y == 0 ? b.x < 0 : b.y < 0; // 偏角が [pi, 2pi) にあるか.
    if(ao != bo) return ao < bo;
    return a.template cross_product<conditional_t<is_void_v<T1>, T, T1>>(b) > 0;
  }
};
inline constexpr comp_arg_t<> comp_arg{};
