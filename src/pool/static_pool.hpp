#pragma once
#include <bits/stdc++.h>
using namespace std;

// 静的配列 + intrusive free list によるメモリプール(index 版).
//   - 静的配列: 確保済みオブジェクトは移動しないので index/参照が安定.
//               木の回転・構築のように「触りながら確保する」用途に向く.
//   - intrusive free list: free したスロットの領域に次の空き index を書く.
//               別コンテナ(stack=deque)を持たず alloc/free が数命令.
//   - 返り値は uint32_t. ポインタ(8B)より軽く相互参照のキャッシュ効率が良い.
//   - slot は union ではなく生バイト領域(std::byte). これにより slot/array/static_pool を
//     trivially destructible に保ち, グローバルプールを「終了時に破棄しない(生存期間が
//     プロセス終了まで続く)」状態にする. 結果, 静的破棄順に依存せず, グローバル木の
//     ~デストラクタが終了時に pool を触っても use-after-lifetime にならない.
//     生存中の T の破棄は free/clear が管理(非trivial T の残存要素は fast 時に破棄を捨てる).
// N: 同時に生存しうる最大要素数.
//    buf を実体で抱えるため sizeof が N*sizeof(slot) と巨大. ローカル変数だと
//    スタックオーバーフローで RE. グローバル or new での確保を前提とする.
template<class T, size_t N = (1 << 20) - 1>
struct static_pool {
  static inline bool fast = false;
  static constexpr uint32_t NIL = uint32_t(-1); // 空きリストの終端 & 無効 index.
  static_assert(N < NIL, "static_pool: N must be < 2^32-1 (index/NIL collision & uint32_t truncation).");

  // 生存中は T, free 中は next(空きリストの index)を同居させる生バイト領域=intrusive.
  // サイズ/アラインは T と uint32_t の max を取る(sizeof(T)<4 でも next 書き込みが溢れない/ズレない).
  // 特別メンバを一切宣言しない=trivially default-constructible/destructible/copyable.
  struct slot {
    alignas(max(alignof(T), alignof(uint32_t))) byte storage[max(sizeof(T), sizeof(uint32_t))];
  };
  array<slot, N> buf;
  uint32_t free_head; // 空きリストの先頭 index. なければ NIL.
  size_t id;          // buf からまだ切り出していない先頭位置.
  size_t cnt;         // 現在生存している要素数.

  constexpr static_pool(): free_head(NIL), id(0), cnt(0) {}

  // ---- 生バイト領域 <-> 型付きオブジェクトの橋渡し(placement 済みを launder で参照) ----
  // launder は placement new/construct_at で生成した実体を指す正当なポインタを得るために必須.
  constexpr T* ptr(uint32_t i) { return launder(reinterpret_cast<T*>(buf[i].storage)); }
  constexpr T const* ptr(uint32_t i) const { return launder(reinterpret_cast<T const*>(buf[i].storage)); }
  // free 中スロットに置いた「次の空き index」への参照.
  constexpr uint32_t& next_of(uint32_t i) { return *launder(reinterpret_cast<uint32_t*>(buf[i].storage)); }

  // O(1)
  template<class... Args>
    requires constructible_from<T, Args...>
  constexpr uint32_t alloc(Args&&... args) {
    ++cnt;
    uint32_t i;
    if(free_head != NIL) { // 空きスロットを再利用(領域は free 時に破棄済みで生バイト).
      i = free_head;
      free_head = next_of(i);
    } else {
      assert(id < N && "static_pool: capacity N exceeded.");
      i = uint32_t(id++);
    }
    construct_at(reinterpret_cast<T*>(buf[i].storage), forward<Args>(args)...);
    return i;
  }
  // O(1)
  constexpr void free(uint32_t i) {
    if constexpr(!is_trivially_destructible_v<T>)
      if(!fast) destroy_at(ptr(i));
    construct_at(reinterpret_cast<uint32_t*>(buf[i].storage), free_head); // storage を next(uint32_t)に張り替える.
    free_head = i;
    --cnt;
  }
  // O(1)
  // buf を先頭から再利用可能にする. 非trivialな型は free 時に個別破棄する設計のため,
  // ここでは破棄しない. よって非trivial型では「全て free 済み(cnt==0)」または
  // 「破棄を捨てる意図で fast=true」でのみ呼ぶこと(それ以外は生存中の破棄漏れ).
  constexpr void clear() {
    if constexpr(!is_trivially_destructible_v<T>)
      assert((fast || cnt == 0) && "static_pool::clear would skip live destructors; free all or set fast.");
    free_head = NIL;
    id = 0;
    cnt = 0;
  }
  constexpr T& operator[](uint32_t i) { return *ptr(i); }
  constexpr T const& operator[](uint32_t i) const { return *ptr(i); }
  constexpr size_t size() const { return cnt; }
};
