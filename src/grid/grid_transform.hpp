#pragma once
#include <bits/stdc++.h>
using namespace std;

// 2次元rangeを転置して返す (H×W → W×H)
template<ranges::random_access_range R,
         ranges::random_access_range Inner = ranges::range_value_t<R>,
         default_initializable T = ranges::range_value_t<Inner>>
  requires constructible_from<R, int, Inner> && constructible_from<Inner, int, T>
R transpose(R const& v) {
  int h = (int)ranges::size(v), w = h ? (int)ranges::size(v[0]) : 0;
  assert(ranges::all_of(v, [w](Inner const& row) { return (int)ranges::size(row) == w; }));
  R res(w, Inner(h, T{}));
  for(int j = 0; j < w; j++)
    for(int i = 0; i < h; i++)
      res[j][i] = v[i][j];
  return res;
}

// 2次元rangeを+90度(時計回り)回転して返す (H×W → W×H)
template<ranges::random_access_range R,
         ranges::random_access_range Inner = ranges::range_value_t<R>,
         default_initializable T = ranges::range_value_t<Inner>>
  requires constructible_from<R, int, Inner> && constructible_from<Inner, int, T>
R rotate90(R const& v) {
  int h = (int)ranges::size(v), w = h ? (int)ranges::size(v[0]) : 0;
  assert(ranges::all_of(v, [w](Inner const& row) { return (int)ranges::size(row) == w; }));
  R res(w, Inner(h, T{}));
  for(int j = 0; j < w; j++)
    for(int i = 0; i < h; i++)
      res[j][h - 1 - i] = v[i][j];
  return res;
}
