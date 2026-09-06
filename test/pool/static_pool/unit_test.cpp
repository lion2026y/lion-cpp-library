// static_pool のディスパッチャ(static_pool.hpp)が約束している「fast 版と検知版は同じ API」
// という契約のコンパイル時テスト. 実行時の挙動ではなく, インターフェースの一致だけを見る.
//   (誤用が assert で検知されることのテストは test/pool/static_pool_checked/unit_test.cpp)
//
// どちらか一方しか include されないので, 1 回のビルドで検査できるのは選ばれた実装だけ.
// 両方を検査するには両モードでビルドすること.
// compile: g++ -std=c++23 -O2 -I<repo>/cpp_library unit_test.cpp               # fast 版
//          g++ -std=c++23 -O2 -DLION_DEBUG -I<repo>/cpp_library unit_test.cpp  # 検知版
// run: ./unit_test
#include "lion/static_pool.hpp"
#include <bits/stdc++.h>
using namespace std;

// 利用側が static_pool に期待してよい操作の全体. 実装を差し替えても再コンパイルだけで
// 済むことを保証するため, 片方にしかない機能に依存しないようここに書いたものが上限になる.
template<class P, class T>
concept static_pool_api = requires(P& p, P const& cp, typename P::id_type h, T v) {
  typename P::id_type;
  requires regular<typename P::id_type>;         // 既定構築/コピー/==
  requires totally_ordered<typename P::id_type>; // set/sort のキーに使える
  { hash<typename P::id_type>{}(h) } -> same_as<size_t>;
  { P::NIL } -> same_as<typename P::id_type const&>;
  { P::fast } -> same_as<bool&>;
  { p.alloc() } -> same_as<typename P::id_type>;
  { p.alloc(v) } -> same_as<typename P::id_type>;
  { p.free(h) } -> same_as<void>;
  { p.clear() } -> same_as<void>;
  { p.ptr(h) } -> same_as<T*>;
  { cp.ptr(h) } -> same_as<T const*>;
  { p[h] } -> same_as<T&>;
  { cp[h] } -> same_as<T const&>;
  { cp.size() } -> same_as<size_t>;
};

// N は契約の検査に関係しないので, 実体化が軽い小さい値で固定する.
static_assert(static_pool_api<static_pool<int, 4>, int>);       // trivial な T.
static_assert(static_pool_api<static_pool<string, 4>, string>); // 非trivial な T.

// 上の concept が空虚に真になっていない(要件を落としても通る状態になっていない)ことの確認.
// free が void を返さない = 差し替え互換でない実装は弾かれなければならない.
template<class T, size_t N>
struct broken_pool {
  using id_type = uint32_t;
  static inline bool fast = false;
  static constexpr id_type NIL = id_type(-1);
  id_type alloc();
  id_type alloc(T);
  bool free(id_type); // ← void ではない.
  void clear();
  T* ptr(id_type);
  T const* ptr(id_type) const;
  T& operator[](id_type);
  T const& operator[](id_type) const;
  size_t size() const;
};
static_assert(!static_pool_api<broken_pool<int, 4>, int>);

int main() {
  cout << "ALL OK\n"; // 検査は全て static_assert なので, ここに来た時点で合格.
  return 0;
}
