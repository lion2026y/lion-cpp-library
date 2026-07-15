#pragma once
#include "lion/static_pool.hpp"
#include "utils.hpp"
#include <bits/stdc++.h>
using namespace std;

// Weight Balanced Leafy Tree
// 葉に要素・内部ノードは集約のみを持つ leafy 木を、重み(葉数)で平衡する.
// メモリは index 版 static_pool(uint32_t ハンドル). 子参照 4B でキャッシュ効率重視.
// merge,split,meld に注意.
//
// 平衡: BB[alpha] 相当. 内部ノード u の子 a,b は w(a)<=D*w(b) && w(b)<=D*w(a) を保つ(D=3).
//       join 中に崩れたら single/double 回転(grandchild の重みを G=2 で比較して選択).
// is_leaf() は sz==1(葉は必ず重み 1, 内部は必ず >=2). union の判別子.
// NIL は「空木の root」だけを意味する(子ハンドルが NIL になることは無い).
// ordered 木は key 一意(map 意味論). 同じ key の insert は値を置換, meld は f で値を統合.
namespace wblt_impl {
// ordered 専用フィールドは「単一の条件付き型」に集約する(同じ空型を 2 つ並べると
// [intro.object]/9 により 0 バイトに畳めないため). K=void(=sequence)で空になる.
template<class K>
struct leafkey {
  K key;
};
template<>
struct leafkey<void> {};
template<class K>
struct router {
  K lo, sep;
}; // lo=部分木の最小 key, sep=右部分木の最小 key(降下の区分キー).
template<>
struct router<void> {};
} // namespace wblt_impl

using bbst::Mode;

// Key=void で sequence(列, ordered=false), Key=非void で ordered(key 順序 map)を表す.
//   sequence:     weight_balanced_leafy_tree<Mode::plain, void, Value>
//   ordered map:  weight_balanced_leafy_tree<Mode::plain, Key, Value>
// Value 既定は Key(ordered set 用). Key=void 時は Value を void にできないので long long を既定にする.
// N は static_pool の容量(同時生存 node 数の上限). 葉数 n の木は最大 2n-1 node を使う.
// pool は同一テンプレート実体で共有・free 分は再利用されるので, 全木の総要素数を n_total とすると
// N >= 2*n_total 程度(split/merge 中の一時 node 分に少し余裕)を確保する. 既定 (1<<20)-1 ≒ 52 万要素.
// Q_e: Q の単位元を返す関数(materialize して静的 e に保持).
template<Mode mode = Mode::plain, class RawKey = void,
         class Value = conditional_t<is_void_v<RawKey>, long long, RawKey>,
         auto Q = plus<Value>{}, auto Q_e = bbst::default_e<Value>,
         auto comp = less<RawKey>{}, size_t N = (1 << 20) - 1>
struct weight_balanced_leafy_tree {
  static constexpr bool ordered = !is_void_v<RawKey>; // RawKey=void なら列, 非void なら key 順序 map.
  // 内部で使う key 型. sequence(ordered=false)では void を直接使うと関数シグネチャが void& を作り
  // ill-formed になるため, 未使用の monostate に差し替える(関数本体は requires(ordered) で呼ばれない).
  using Key = conditional_t<ordered, RawKey, monostate>;
  static constexpr bool lazy = mode != Mode::plain;
  static constexpr bool beats = mode == Mode::beats;
  // static_assert は require より診断メッセージが具体的なので残す.
  static_assert(!is_void_v<Value>, "Value must not be void (specify it for sequence trees)");
  static_assert(is_invocable_r_v<Value, decltype(Q), Value, Value>, "Q must work as Value(Value,Value)");
  static_assert(is_invocable_r_v<Value, decltype(Q_e)>, "Q_e must work as Value()");
  static_assert(!ordered || is_invocable_r_v<bool, decltype(comp), Key, Key>, "comp must work as bool(Key,Key)");
  // union に入るのは Key のみ(val は union 外). 非 trivial Key は node の生存管理が壊れるため非対応.
  static_assert(!ordered || is_trivially_destructible_v<Key>, "Key must be trivially destructible");
  static_assert(!lazy && !beats, "lazy/beats not implemented yet (Phase 2/3)");

public:
  using ID = bbst::ID;
  struct node {
    Value val;
    int sz; // 部分木の葉数 = 重み. sz==1 ⟺ 葉.
    union {
      [[no_unique_address]] wblt_impl::leafkey<RawKey> lf;
      struct {
        uint32_t ch[2];
        [[no_unique_address]] wblt_impl::router<RawKey> r;
      } in;
    };
    node() {}
  };

private:
  static constexpr uint32_t NIL = static_pool<node, N>::NIL;
  static constexpr int D = 3, G = 2; // 平衡パラメータ (delta, gamma).
  // DFS スタックの上界. 重み平衡なので高さ <= log_{(D+1)/D}(葉数) <= 約 2.41*log2(N). 余裕を見て確保.
  // 固定長ローカル array で使うので, static を持たず(破棄順問題なし)・ヒープ確保もゼロ.
  static constexpr int STACK_CAP = 3 * bit_width(N) + 4;
  static inline Value const e = Q_e();
  static inline static_pool<node, N> pool;
  uint32_t root;

  // ---- node 基本操作 ----
  static constexpr bool is_leaf(uint32_t i) { return pool[i].sz == 1; }
  static constexpr int w(uint32_t i) { return pool[i].sz; }
  static constexpr uint32_t child(uint32_t u, bool d) { return pool[u].in.ch[d]; }
  static constexpr bool eqk(Key const& a, Key const& b) { return !comp(a, b) && !comp(b, a); }
  // 部分木の最小 key(ordered 時のみ).
  static constexpr Key const& lo(uint32_t i) { return is_leaf(i) ? pool[i].lf.key : pool[i].in.r.lo; }
  static constexpr uint32_t new_leaf(Value const& v) {
    uint32_t u = pool.alloc();
    pool[u].sz = 1;
    pool[u].val = v;
    return u;
  }
  static constexpr uint32_t new_leaf_key(Key const& k, Value const& v)
    requires(ordered)
  {
    uint32_t u = new_leaf(v);
    pool[u].lf.key = k;
    return u;
  }
  // 集約を子から引き上げる. u は内部ノード.
  static constexpr void update(uint32_t u) {
    node& n = pool[u];
    uint32_t l = n.in.ch[0], r = n.in.ch[1];
    n.sz = pool[l].sz + pool[r].sz;
    n.val = Q(pool[l].val, pool[r].val);
    if constexpr(ordered) n.in.r.lo = lo(l), n.in.r.sep = lo(r);
  }
  // l,r を子に持つ内部ノードを新規作成(平衡は呼び出し側が保証).
  static constexpr uint32_t make(uint32_t l, uint32_t r) {
    uint32_t u = pool.alloc();
    pool[u].in.ch[0] = l;
    pool[u].in.ch[1] = r;
    update(u);
    return u;
  }

  // ---- 木操作コア: merge(join) / split / build / clone ----
  // l,r をこの順に連結(join). 片方が空なら他方. 平衡を保つ根を返す. O(logN).
  static constexpr uint32_t merge(uint32_t l, uint32_t r) {
    if(l == NIL) return r;
    if(r == NIL) return l;
    return merge_sub(l, r);
  }
  template<class... Args>
  static constexpr uint32_t merge(uint32_t a, Args... rest) { return merge(a, merge(rest...)); }
  static constexpr uint32_t merge_sub(uint32_t l, uint32_t r) { // l,r ともに非 NIL.
    if(D * w(r) < w(l)) {                                       // l が重すぎ: l の右 spine を降下.
      uint32_t a = child(l, 0), c = merge_sub(child(l, 1), r);
      if(w(c) <= D * w(a)) {
        pool[l].in.ch[1] = c, update(l);
        return l;
      }
      if(w(child(c, 0)) <= G * w(child(c, 1))) { // single rotation.
        pool[l].in.ch[1] = child(c, 0), update(l);
        pool[c].in.ch[0] = l, update(c);
        return c;
      }
      uint32_t cl = child(c, 0); // double rotation.
      pool[l].in.ch[1] = child(cl, 0);
      pool[c].in.ch[0] = child(cl, 1);
      update(l), update(c);
      pool[cl].in.ch[0] = l, pool[cl].in.ch[1] = c, update(cl);
      return cl;
    }
    if(D * w(l) < w(r)) { // r が重すぎ: 対称.
      uint32_t b = child(r, 1), c = merge_sub(l, child(r, 0));
      if(w(c) <= D * w(b)) {
        pool[r].in.ch[0] = c, update(r);
        return r;
      }
      if(w(child(c, 1)) <= G * w(child(c, 0))) { // single rotation.
        pool[r].in.ch[0] = child(c, 1), update(r);
        pool[c].in.ch[1] = r, update(c);
        return c;
      }
      uint32_t cr = child(c, 1); // double rotation.
      pool[r].in.ch[0] = child(cr, 1);
      pool[c].in.ch[1] = child(cr, 0);
      update(r), update(c);
      pool[cr].in.ch[1] = r, pool[cr].in.ch[0] = c, update(cr);
      return cr;
    }
    return make(l, r);
  }
  // u を先頭 k 個 / 残りに分ける(index split). u を消費して 2 根を返す. O(logN).
  static constexpr pair<uint32_t, uint32_t> split(uint32_t u, int k) {
    if(u == NIL) return {NIL, NIL};
    if(k <= 0) return {NIL, u};
    if(k >= w(u)) return {u, NIL};
    return split_sub(u, k);
  }
  static constexpr pair<uint32_t, uint32_t> split_sub(uint32_t u, int k) { // 0<k<w(u).
    uint32_t l = child(u, 0), r = child(u, 1);
    int wl = w(l);
    pool.free(u);
    if(k < wl) {
      auto [a, b] = split_sub(l, k);
      return {a, merge(b, r)};
    }
    if(k > wl) {
      auto [a, b] = split_sub(r, k - wl);
      return {merge(l, a), b};
    }
    return {l, r};
  }
  static constexpr uint32_t build(vector<Value> const& v, int l, int r) { // vector から平衡木を構築. O(N).
    if(l >= r) return NIL;
    if(l + 1 == r) return new_leaf(v[l]);
    int m = (l + r) / 2;
    return make(build(v, l, m), build(v, m, r));
  }
  static constexpr uint32_t build_key(vector<pair<Key, Value>> const& v, int l, int r)
    requires(ordered)
  {
    if(l >= r) return NIL;
    if(l + 1 == r) return new_leaf_key(v[l].first, v[l].second);
    int m = (l + r) / 2;
    return make(build_key(v, l, m), build_key(v, m, r));
  }
  // 部分木を丸ごと複製(deep copy). O(部分木サイズ).
  static constexpr uint32_t clone(uint32_t u) {
    if(u == NIL) return NIL;
    uint32_t v = pool.alloc();
    pool[v] = pool[u];
    if(!is_leaf(u)) pool[v].in.ch[0] = clone(child(u, 0)), pool[v].in.ch[1] = clone(child(u, 1));
    return v;
  }
  static constexpr void free_all(uint32_t u) { // 部分木の全 node を free(固定長ローカル array で DFS).
    array<uint32_t, STACK_CAP> st;
    int top = 0;
    st[top++] = u;
    while(top) {
      uint32_t x = st[--top];
      if(!is_leaf(x)) assert(top + 2 <= STACK_CAP), st[top++] = child(x, 0), st[top++] = child(x, 1);
      pool.free(x);
    }
  }
  // 葉から順に f(葉ハンドル) を呼ぶ左→右 DFS(get_vec 系の共有).
  static constexpr void each_leaf(uint32_t u, auto const& f) {
    array<uint32_t, STACK_CAP> st;
    int top = 0;
    st[top++] = u;
    while(top) {
      uint32_t x = st[--top];
      if(is_leaf(x)) f(x);
      else assert(top + 2 <= STACK_CAP), st[top++] = child(x, 1), st[top++] = child(x, 0);
    }
  }

  // ---- モード横断の下請け(index を扱うコア) ----
  static constexpr uint32_t leaf_at(uint32_t u, int k) { // index k の葉ハンドル.
    while(!is_leaf(u)) {
      int wl = w(child(u, 0));
      if(k < wl) u = child(u, 0);
      else k -= wl, u = child(u, 1);
    }
    return u;
  }
  // index k の値を g(値) に変更し祖先を pull-up(set は定数 g, modify は f). key 側 update_key と対.
  static constexpr void update_sub(uint32_t u, int k, auto const& g) {
    if(is_leaf(u)) {
      pool[u].val = g(pool[u].val);
      return;
    }
    int wl = w(child(u, 0));
    if(k < wl) update_sub(child(u, 0), k, g);
    else update_sub(child(u, 1), k - wl, g);
    update(u);
  }
  static constexpr uint32_t erase_at(uint32_t u, int k) { // index k の葉を削除. 1 パス. O(logN).
    if(is_leaf(u)) {
      pool.free(u);
      return NIL;
    }
    uint32_t l = child(u, 0), r = child(u, 1);
    int wl = w(l);
    pool.free(u);
    if(k < wl) return merge(erase_at(l, k), r);
    return merge(l, erase_at(r, k - wl));
  }
  static constexpr Value prod_sub(uint32_t u, int l, int r) { // [l,r)∩[0,w(u)) の総積(非空前提).
    if(l <= 0 && r >= w(u)) return pool[u].val;
    uint32_t a = child(u, 0), b = child(u, 1);
    int wl = w(a);
    if(r <= wl) return prod_sub(a, l, r);
    if(l >= wl) return prod_sub(b, l - wl, r - wl);
    return Q(prod_sub(a, l, wl), prod_sub(b, 0, r - wl));
  }
  // u の先頭から f を保つ最大の右端 r を返す(木上二分探索, 確保なしの top-down). acc は流入積.
  static constexpr int mr(uint32_t u, int l, Value& acc, auto const& f) {
    if(l <= 0) {
      Value na = Q(acc, pool[u].val);
      if(f(na)) {
        acc = na;
        return w(u);
      }
    }
    if(is_leaf(u)) return 0;
    int wl = w(child(u, 0));
    if(l >= wl) return wl + mr(child(u, 1), l - wl, acc, f);
    int lc = mr(child(u, 0), l, acc, f);
    if(lc < wl) return lc;
    return wl + mr(child(u, 1), 0, acc, f);
  }
  // u の [0,r) 内で f を保つ最小の左端 l を返す. acc は流入積.
  static constexpr int ml(uint32_t u, int r, Value& acc, auto const& f) {
    if(r >= w(u)) {
      Value na = Q(pool[u].val, acc);
      if(f(na)) {
        acc = na;
        return 0;
      }
    }
    if(is_leaf(u)) return 1;
    int wl = w(child(u, 0));
    if(r <= wl) return ml(child(u, 0), r, acc, f);
    int rc = ml(child(u, 1), r - wl, acc, f);
    if(rc > 0) return wl + rc;
    return ml(child(u, 0), wl, acc, f);
  }
  constexpr int max_right_at(int l, auto const& f) const {
    assert(0 <= l && l <= size());
    assert(f(e));
    if(l == size()) return size();
    Value acc = e;
    return mr(root, l, acc, f);
  }
  constexpr int min_left_at(int r, auto const& f) const {
    assert(0 <= r && r <= size());
    assert(f(e));
    if(r == 0) return 0;
    Value acc = e;
    return ml(root, r, acc, f);
  }

  // ---- ordered 専用の下請け(key を扱うコア) ----
  // le=false: count key<k(lower_bound) / le=true: count key<=k(upper_bound).
  constexpr int bound(Key const& k, bool le) const {
    if(root == NIL) return 0;
    uint32_t u = root;
    int idx = 0;
    while(!is_leaf(u)) {
      Key const& sp = pool[u].in.r.sep;
      bool go_left = le ? comp(k, sp) : !comp(sp, k);
      if(go_left) u = child(u, 0);
      else idx += w(child(u, 0)), u = child(u, 1);
    }
    Key const& lk = pool[u].lf.key;
    if(le ? !comp(k, lk) : comp(lk, k)) ++idx;
    return idx;
  }
  // key=k が入るべき葉と, その葉の index を 1 降下で返す(exact search). key 一意なので唯一.
  static constexpr pair<uint32_t, int> find_leaf_key(uint32_t u, Key const& k) {
    int idx = 0;
    while(!is_leaf(u)) {
      if(comp(k, pool[u].in.r.sep)) u = child(u, 0);
      else idx += w(child(u, 0)), u = child(u, 1);
    }
    return {u, idx};
  }
  // key=k の葉の val を g(val) に変更(1 降下で祖先 pull-up). 見つかったか返す(set/modify 共有).
  static constexpr bool update_key(uint32_t u, Key const& k, auto const& g) {
    if(is_leaf(u)) {
      if(!eqk(k, pool[u].lf.key)) return false;
      pool[u].val = g(pool[u].val);
      return true;
    }
    bool ok = comp(k, pool[u].in.r.sep) ? update_key(child(u, 0), k, g) : update_key(child(u, 1), k, g);
    if(ok) update(u);
    return ok;
  }
  // key ∈ [kl,kr) の総積(1 降下). khi=u の keys の strict upper(nullptr=+inf, key 一意前提).
  static constexpr Value prod_key(uint32_t u, Key const& kl, Key const& kr, Key const* khi) {
    Key const& klo = lo(u);
    if(!comp(klo, kr)) return e;                                     // klo>=kr: 全て範囲外(上).
    if(khi && !comp(kl, *khi)) return e;                             // *khi<=kl: 全て範囲外(下).
    if(!comp(klo, kl) && khi && !comp(kr, *khi)) return pool[u].val; // 全部含む.
    if(is_leaf(u)) return comp(klo, kl) ? e : pool[u].val;           // 部分葉: klo<kl 除外.
    return Q(prod_key(child(u, 0), kl, kr, &pool[u].in.r.sep), prod_key(child(u, 1), kl, kr, khi));
  }
  // key<s | key==s(単一 or NIL) | key>s に分ける.
  static constexpr tuple<uint32_t, uint32_t, uint32_t> split_key3(uint32_t u, Key const& s) {
    auto [l, ge] = split_key(u, s);
    if(ge != NIL && eqk(lo(ge), s)) {
      auto [eq, gt] = split(ge, 1);
      return {l, eq, gt};
    }
    return {l, NIL, ge};
  }
  static constexpr pair<uint32_t, uint32_t> split_key(uint32_t u, Key const& s) { // key<s | key>=s.
    if(u == NIL) return {NIL, NIL};
    if(is_leaf(u)) return comp(pool[u].lf.key, s) ? pair<uint32_t, uint32_t>{u, NIL} : pair<uint32_t, uint32_t>{NIL, u};
    uint32_t l = child(u, 0), r = child(u, 1);
    Key sp = pool[u].in.r.sep; // free 後に使うのでコピー必須.
    pool.free(u);
    if(!comp(sp, s)) { // sp>=s: 境界は左.
      auto [a, b] = split_key(l, s);
      return {a, merge(b, r)};
    }
    auto [a, b] = split_key(r, s);
    return {merge(l, a), b};
  }
  // 葉 lf(単一要素)を map t に統合挿入. key 衝突時 lf.val = f(lf.val, 既存.val). O(log|t|).
  static constexpr uint32_t insert_leaf(uint32_t t, uint32_t lf, auto const& f) {
    auto [l, m, r] = split_key3(t, pool[lf].lf.key);
    if(m != NIL) pool[lf].val = f(pool[lf].val, pool[m].val), pool.free(m);
    return merge(l, lf, r);
  }
  // key 一意の map として union. どちらかが葉ならそれを他方へ挿入(高速化). 両内部なら大きい方で split.
  static constexpr uint32_t meld_sub(uint32_t a, uint32_t b, auto const& f) {
    if(a == NIL) return b;
    if(b == NIL) return a;
    if(is_leaf(a)) return insert_leaf(b, a, f);
    if(is_leaf(b)) return insert_leaf(a, b, f);
    if(w(a) < w(b)) swap(a, b); // 大きい方 a を分割軸に(小さい方 b を回す).
    Key s = pool[a].in.r.sep;
    uint32_t al = child(a, 0), ar = child(a, 1);
    pool.free(a);
    auto [bl, br] = split_key(b, s);
    return merge(meld_sub(al, bl, f), meld_sub(ar, br, f));
  }
  static constexpr int height_sub(uint32_t u) {
    return is_leaf(u) ? 1 : 1 + max(height_sub(child(u, 0)), height_sub(child(u, 1)));
  }

public:
  // static_pool::fast への参照. true で自動破棄を省略(=生存要素の Value dtor を捨てる高速化).
  static inline bool& fast = static_pool<node, N>::fast;

  constexpr weight_balanced_leafy_tree(): root(NIL) {}
  constexpr weight_balanced_leafy_tree(vector<Value> const& v)
    requires(!ordered)
      : root(build(v, 0, v.size())) {}
  // sorted でなければ stable_sort. key は一意でなければ assert(map 意味論).
  constexpr weight_balanced_leafy_tree(vector<pair<Key, Value>> v)
    requires(ordered)
      : root(NIL) {
    stable_sort(v.begin(), v.end(), [](auto const& x, auto const& y) { return comp(x.first, y.first); });
    for(size_t i = 1; i < v.size(); ++i) assert(!eqk(v[i - 1].first, v[i].first) && "duplicate keys not allowed");
    root = build_key(v, 0, v.size());
  }
  // copy は deep copy(node を共有しない). O(N).
  constexpr weight_balanced_leafy_tree(weight_balanced_leafy_tree const& ot): root(clone(ot.root)) {}
  constexpr weight_balanced_leafy_tree& operator=(weight_balanced_leafy_tree const& ot) {
    if(this != &ot) {
      if(!fast) clear();
      root = clone(ot.root);
    }
    return *this;
  }
  constexpr weight_balanced_leafy_tree(weight_balanced_leafy_tree&& ot) noexcept: root(ot.root) { ot.root = NIL; }
  constexpr weight_balanced_leafy_tree& operator=(weight_balanced_leafy_tree&& ot) noexcept {
    if(this != &ot) {
      if(!fast) clear();
      root = ot.root, ot.root = NIL;
    }
    return *this;
  }
  // fast=true は破棄を捨てる高速化のみ.
  constexpr ~weight_balanced_leafy_tree() {
    if(!fast) clear();
  }

  constexpr int size() const { return root == NIL ? 0 : pool[root].sz; }
  constexpr bool empty() const { return root == NIL; }
  constexpr Value all_prod() const { return root == NIL ? e : pool[root].val; }
  // この木の全 node を free して空にする(fast によらず free する. fast 判定は呼び出し側). O(N).
  constexpr void clear() {
    if(root != NIL) free_all(root);
    root = NIL;
  }
  // 木の高さ(デバッグ・検証用). O(N).
  constexpr int height() const { return root == NIL ? 0 : height_sub(root); } // デバッグ用. O(N).

  // ===== index API(sequence, ordered=false). =====
  // O(logN) index k の値.
  constexpr Value const& get(int k) const
    requires(!ordered)
  {
    assert(0 <= k && k < size());
    return pool[leaf_at(root, k)].val;
  }
  // O(logN) index k の値を v に.
  constexpr void set(int k, Value const& v)
    requires(!ordered)
  {
    assert(0 <= k && k < size());
    update_sub(root, k, [&](Value const&) { return v; });
  }
  // O(logN) index k の値を f(値) に置換(名前 apply は遅延の区間作用に予約).
  template<class F>
    requires(!ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify(int k, F const& f) {
    assert(0 <= k && k < size());
    update_sub(root, k, f);
  }
  // O(logN) index k の手前に v を挿入(挿入後に k 番目).
  constexpr void insert(int k, Value const& v)
    requires(!ordered)
  {
    assert(0 <= k && k <= size());
    auto [l, r] = split(root, k);
    root = merge(l, new_leaf(v), r);
  }
  // O(logN) index k の要素を削除.
  constexpr void erase(int k)
    requires(!ordered)
  {
    assert(0 <= k && k < size());
    root = erase_at(root, k);
  }
  // O(logN) [l,r) の総積.
  constexpr Value prod(int l, int r) const
    requires(!ordered)
  {
    assert(0 <= l && l <= r && r <= size());
    if(l >= r) return e;
    return prod_sub(root, l, r);
  }
  // O(logN) f(prod([l,r)))=true なる最大の r(引数省略で l=0). f(e)=true, 単調前提.
  template<class F>
    requires(!ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int max_right(F const& f) const { return max_right_at(0, f); }
  template<class F>
    requires(!ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int max_right(F const& f, int l) const { return max_right_at(l, f); }
  // O(logN) f(prod([l,r)))=true なる最小の l(引数省略で r=size). f(e)=true, 単調前提.
  template<class F>
    requires(!ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int min_left(F const& f) const { return min_left_at(size(), f); }
  template<class F>
    requires(!ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int min_left(F const& f, int r) const { return min_left_at(r, f); }
  // O(N)
  constexpr vector<Value> get_vec() const {
    vector<Value> ret;
    ret.reserve(size());
    if(root != NIL) each_leaf(root, [&](uint32_t u) { ret.push_back(pool[u].val); });
    return ret;
  }

  // ===== ordered-key API(順序集合, key 一意=map 意味論). =====
  // O(logN) key<k の要素数(lower_bound の index).
  constexpr int lower_bound(Key const& k) const
    requires(ordered)
  {
    return bound(k, false);
  }
  // O(logN) key<=k の要素数(upper_bound の index).
  constexpr int upper_bound(Key const& k) const
    requires(ordered)
  {
    return bound(k, true);
  }
  // O(logN) key=k が存在するか.
  constexpr bool contains(Key const& k) const
    requires(ordered)
  {
    if(root == NIL) return false;
    auto [u, idx] = find_leaf_key(root, k);
    return eqk(k, pool[u].lf.key);
  }
  // O(logN) key=k の値. 存在しなければ assert 失敗.
  constexpr Value const& get(Key const& k) const
    requires(ordered)
  {
    assert(root != NIL);
    auto [u, idx] = find_leaf_key(root, k);
    assert(eqk(k, pool[u].lf.key) && "get(Key): key not found");
    return pool[u].val;
  }
  // O(logN) key=k の値. 無ければ def(既定 e)を返す.
  constexpr Value get_or(Key const& k, Value const& def = e) const
    requires(ordered)
  {
    if(root == NIL) return def;
    auto [u, idx] = find_leaf_key(root, k);
    return eqk(k, pool[u].lf.key) ? pool[u].val : def;
  }
  // O(logN) key=k の値を v に. 存在しなければ assert 失敗.
  constexpr void set(Key const& k, Value const& v)
    requires(ordered)
  {
    bool ok = root != NIL && update_key(root, k, [&](Value const&) { return v; });
    assert(ok && "set(Key,Value): key not found");
    (void)ok;
  }
  // O(logN) key=k の値を f(値) に置換. 存在しなければ assert 失敗(名前 apply は遅延の区間作用に予約).
  template<class F>
    requires(ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify(Key const& k, F const& f) {
    bool ok = root != NIL && update_key(root, k, f);
    assert(ok && "modify(Key,f): key not found");
    (void)ok;
  }
  // O(logN) key=k があれば f(値) に置換, 無ければ f(e) を挿入(upsert). 存在時はその場更新で速い.
  template<class F>
    requires(ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify_or(Key const& k, F const& f) {
    if(root != NIL && update_key(root, k, f)) return; // 存在 → その場更新.
    auto [l, eq, r] = split_key3(root, k);            // 不在 → f(e) を挿入(eq==NIL).
    root = merge(l, new_leaf_key(k, f(e)), r);
  }
  // O(logN) key=k を挿入. 既に存在すれば値を v に置換(update-or-insert なので名前は upsert).
  constexpr void upsert(Key const& k, Value const& v)
    requires(ordered)
  {
    auto [l, eq, r] = split_key3(root, k);
    if(eq != NIL) pool.free(eq);
    root = merge(l, new_leaf_key(k, v), r);
  }
  // O(logN) key=k を削除. 存在したかを返す.
  constexpr bool erase(Key const& k)
    requires(ordered)
  {
    if(root == NIL) return false;
    auto [u, idx] = find_leaf_key(root, k);
    if(!eqk(k, pool[u].lf.key)) return false;
    root = erase_at(root, idx);
    return true;
  }
  // O(logN) key ∈ [kl,kr) の総積(1 降下).
  constexpr Value prod(Key const& kl, Key const& kr) const
    requires(ordered)
  {
    if(root == NIL || !comp(kl, kr)) return e;
    return prod_key(root, kl, kr, nullptr);
  }
  // O(logN) key=kl 以上を左端に固定した木上二分探索. 返り値は index.
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int max_right(F const& f, Key const& kl) const { return max_right_at(lower_bound(kl), f); }
  // O(logN) key=kr 未満を右端に固定した木上二分探索. 返り値は index.
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int min_left(F const& f, Key const& kr) const { return min_left_at(lower_bound(kr), f); }
  // O(M log(1+N/M)) ot を this に union. key 衝突は f で統合(内部で small-to-large するため
  // 引数順は不定 → f は可換推奨. 既定 Q). ot は空に. f は Value(Value,Value).
  template<class F = decltype(Q)>
    requires(ordered && is_invocable_r_v<Value, F, Value, Value>)
  constexpr void meld(weight_balanced_leafy_tree& ot, F const& f = Q) {
    if(this == &ot) return;
    root = meld_sub(root, ot.root, f);
    ot.root = NIL;
  }
  // O(N) 中身は pool 内 node への参照(木を変更するまで有効).
  constexpr vector<pair<Key const&, Value const&>> get_vec_pair() const
    requires(ordered)
  {
    vector<pair<Key const&, Value const&>> ret;
    ret.reserve(size());
    if(root != NIL) each_leaf(root, [&](uint32_t u) { ret.emplace_back(pool[u].lf.key, pool[u].val); });
    return ret;
  }

  // ===== ordered-index(ID)API. ID(i) で key と型ディスパッチ. =====
  // O(logN) index i の値.
  constexpr Value const& get(ID i) const
    requires(ordered)
  {
    assert(0 <= i.id && i.id < size());
    return pool[leaf_at(root, i.id)].val;
  }
  // O(logN) index i の key.
  constexpr Key const& key_at(int i) const
    requires(ordered)
  {
    assert(0 <= i && i < size());
    return pool[leaf_at(root, i)].lf.key;
  }
  // O(logN) index i の値のみ変更(key 不変なので順序は保たれる).
  constexpr void set(ID i, Value const& v)
    requires(ordered)
  {
    assert(0 <= i.id && i.id < size());
    update_sub(root, i.id, [&](Value const&) { return v; });
  }
  // O(logN) index i の値を f(値) に置換(名前 apply は遅延の区間作用に予約).
  template<class F>
    requires(ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify(ID i, F const& f) {
    assert(0 <= i.id && i.id < size());
    update_sub(root, i.id, f);
  }
  // O(logN) index i の要素を削除.
  constexpr void erase(ID i)
    requires(ordered)
  {
    assert(0 <= i.id && i.id < size());
    root = erase_at(root, i.id);
  }
  // O(logN) index ∈ [l,r) の総積.
  constexpr Value prod(ID l, ID r) const
    requires(ordered)
  {
    assert(0 <= l.id && l.id <= r.id && r.id <= size());
    if(l.id >= r.id) return e;
    return prod_sub(root, l.id, r.id);
  }
  // O(logN) index を左端に固定した木上二分探索(引数省略で l=0).
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int max_right(F const& f) const { return max_right_at(0, f); }
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int max_right(F const& f, ID l) const { return max_right_at(l.id, f); }
  // O(logN) index を右端に固定した木上二分探索(引数省略で r=size).
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int min_left(F const& f) const { return min_left_at(size(), f); }
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int min_left(F const& f, ID r) const { return min_left_at(r.id, f); }
};
