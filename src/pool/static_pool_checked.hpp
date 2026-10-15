#pragma once
#include <bits/stdc++.h>
using namespace std;

// static_pool の誤用検知版(速度は不問. LION_DEBUG 時に static_pool.hpp から差し替えられる).
// fast 版と同じ API を持つが, 内部構造を捨てて「壊れた使い方」を assert で即座に落とすことに全振りする.
// 検知できる/できない誤用, 検知の仕組み, メモリ量は document/static_pool_checked.md を参照.
// fast 版同様 trivially destructible を保つ(メンバはスカラ/array だけにする).
// N: 同時に生存しうる最大要素数.

// ハンドル型. fast 版の uint32_t に対し, 検知版は {スロット index, 世代} の 8B の構造体.
// 世代を index に詰め込まず独立した uint32_t にしてあるので, N を大きく取っても世代が枯渇しない.
// T 非依存の名前空間スコープ型にしてあるのは, pool<T> の T(node 等)を定義する前に
// ハンドル型を参照したい(node が子ハンドルを持つ)という循環依存を避けるため.
// fast 版の id_type は uint32_t なので set/map のキーやソートに使える. 実装を差し替えただけで
// 利用側が壊れないよう, 検知版でも順序比較と std::hash を提供する(値の大小に意味はない).
struct static_pool_id {
  uint32_t idx, gen;
  friend constexpr bool operator==(static_pool_id, static_pool_id) = default;
  friend constexpr auto operator<=>(static_pool_id, static_pool_id) = default;
};
template<>
struct std::hash<static_pool_id> {
  size_t operator()(static_pool_id h) const noexcept {
    return std::hash<uint64_t>{}(uint64_t(h.idx) << 32 | h.gen);
  }
};

template<class T, size_t N = (1 << 20) - 1>
struct static_pool {
  static inline bool fast = false;
  using id_type = static_pool_id;
  // 無効ハンドル. どのスロットの生きた世代とも一致しない値.
  static constexpr id_type NIL = {uint32_t(-1), uint32_t(-1)};
  static_assert(N < uint32_t(-1), "static_pool: N must be < 2^32-1 (index/NIL collision & uint32_t truncation).");

  // 生バイト領域. 検知版は free リストを同居させないので sizeof(T) だけあればよい.
  // 特別メンバを宣言しない = trivially default-constructible/destructible.
  struct slot {
    alignas(T) byte storage[sizeof(T)];
  };
  enum : uint8_t { UNBORN = 0, // まだ一度も切り出していない.
                   LIVE,       // T が生存中.
                   FREED };    // free 済み(storage は毒値).
  static constexpr byte POISON{0xDD};

  array<slot, N> buf;
  array<uint32_t, N> gen;        // スロットの現世代. free/clear のたびに進む(巻き戻さない).
  array<uint8_t, N> state;       // UNBORN/LIVE/FREED.
  array<uint32_t, N> free_slots; // 空きスロットの置き場(storage の外なので stale 書き込みで壊れない).
  size_t free_top;               // free_slots の有効長.
  size_t id;                     // buf からまだ切り出していない先頭位置.
  size_t cnt;                    // 現在生存している要素数.

  constexpr static_pool(): gen{}, state{}, free_top(0), id(0), cnt(0) {}

  // ---- 生バイト領域 <-> 型付きオブジェクトの橋渡し ----
  // launder は placement new/construct_at で生成した実体を指す正当なポインタを得るために必須.
  constexpr T* raw_ptr(uint32_t i) { return launder(reinterpret_cast<T*>(buf[i].storage)); }
  constexpr T const* raw_ptr(uint32_t i) const { return launder(reinterpret_cast<T const*>(buf[i].storage)); }

  // ハンドルの正当性検査. 生きているスロットを指していなければ assert で落とす.
  // 「参照」と「free」で通すべき条件は完全に同じなので, 検査はここ 1 箇所に集約する
  // (2 箇所に書くと片方だけ直したときに検知条件が食い違うため).
  constexpr uint32_t check(id_type h) const {
    assert(!(h == NIL) && "static_pool: NIL handle.");
    assert(h.idx < id && "static_pool: invalid handle (never allocated / out of range / used after clear).");
    uint32_t i = h.idx;
    // ここで state[i] は LIVE か FREED のいずれか(id は state=LIVE と同時にしか進まず,
    // clear は id=0 と UNBORN 化を同時に行うので, i<id かつ UNBORN にはならない).
    // また free は必ず gen を進めるので, 世代が一致するなら そのスロットは LIVE である.
    assert(!(state[i] == FREED && h.gen + 1 == gen[i]) && "static_pool: freed slot (use after free / double free).");
    assert(h.gen == gen[i] && "static_pool: stale handle (slot was freed and reallocated).");
    return i;
  }
  // free 済み領域に置いた毒値が無傷か検査する(再確保の直前に呼ぶ).
  // 壊れていれば「free 後に stale なポインタ/参照経由で書き込まれた」ということ.
  constexpr void check_poison(uint32_t i) const {
    for(size_t k = 0; k < sizeof(T); ++k)
      assert(buf[i].storage[k] == POISON && "static_pool: write to freed slot (dangling pointer).");
  }

  // O(sizeof(T))
  template<class... Args>
    requires constructible_from<T, Args...>
  constexpr id_type alloc(Args&&... args) {
    ++cnt;
    uint32_t i;
    if(free_top) { // 空きスロットを再利用(順序は保証しない).
      i = free_slots[--free_top];
      check_poison(i);
    } else {
      assert(id < N && "static_pool: capacity N exceeded.");
      i = uint32_t(id++);
    }
    construct_at(reinterpret_cast<T*>(buf[i].storage), forward<Args>(args)...);
    state[i] = LIVE;
    return {i, gen[i]};
  }
  // O(sizeof(T))
  constexpr void free(id_type h) {
    uint32_t i = check(h); // 参照時と同じ検査(二重 free / 再確保後の free もここで落ちる).
    if constexpr(!is_trivially_destructible_v<T>)
      if(!fast) destroy_at(raw_ptr(i));
    fill_n(buf[i].storage, sizeof(T), POISON); // 以後の読み書きを毒値破壊として検知するため.
    state[i] = FREED;
    ++gen[i]; // 再確保しても古いハンドルが一致しないようにする.
    assert(gen[i] != uint32_t(-1) && "static_pool: generation exhausted (2^32-1 frees on one slot).");
    free_slots[free_top++] = i;
    --cnt;
  }
  // O(id)
  // buf を先頭から再利用可能にする. 非trivialな型は free 時に個別破棄する設計のため,
  // ここでは破棄しない. よって非trivial型では「全て free 済み(cnt==0)」または
  // 「破棄を捨てる意図で fast=true」でのみ呼ぶこと(それ以外は生存中の破棄漏れ).
  constexpr void clear() {
    if constexpr(!is_trivially_destructible_v<T>)
      assert((fast || cnt == 0) && "static_pool::clear would skip live destructors; free all or set fast.");
    // 世代は巻き戻さずに進める. これで clear を跨いだ古いハンドルも検知できる.
    for(size_t i = 0; i < id; ++i) {
      if(state[i] == LIVE) ++gen[i];
      state[i] = UNBORN;
    }
    free_top = 0;
    id = 0;
    cnt = 0;
  }
  constexpr T* ptr(id_type h) { return raw_ptr(check(h)); }
  constexpr T const* ptr(id_type h) const { return raw_ptr(check(h)); }
  constexpr T& operator[](id_type h) { return *ptr(h); }
  constexpr T const& operator[](id_type h) const { return *ptr(h); }
  constexpr size_t size() const { return cnt; }
};
