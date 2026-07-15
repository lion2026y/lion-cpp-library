#pragma once
#include <bits/stdc++.h>
using namespace std;

// 平衡二分探索木で共有するユーティリティ.
namespace bbst {
// モノイド単位元(既定): T{}.
template<class T>
constexpr T default_e() { return T{}; }

// 集約の遅延レベル: plain(なし) ⊂ lazy(区間作用) ⊂ beats.
enum class Mode { plain,
                  lazy,
                  beats };

// index(位置)アクセスを key と型で区別するためのラッパ.
//   ordered 木: prod(kl, kr)=key 版 / prod(ID(l), ID(r))=index 版.
struct ID {
  int id;
  explicit constexpr ID(int i): id(i) {}
};
} // namespace bbst
