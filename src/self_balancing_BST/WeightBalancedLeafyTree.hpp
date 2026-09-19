#pragma once
#include "lion/static_pool.hpp"
#include "utils.hpp"
#include <bits/stdc++.h>
using namespace std;

// Weight Balanced Leafy Tree
// 葉に要素・内部ノードは集約のみを持つ leafy 木を、重み(葉数)で平衡する.
// メモリは index 版 static_pool(ハンドル型 id_type). 通常ビルドでは id_type=uint32_t で子参照 4B,
// キャッシュ効率重視. LION_DEBUG では誤用検知版 pool に差し替わり id_type も世代付きになる.
// merge,split,meld に注意.
//
// 平衡: BB[alpha] 相当. 内部ノード u の子 a,b は w(a)<=delta*w(b) && w(b)<=delta*w(a) を保つ(delta=5/2).
//       join 中に崩れたら single/double 回転(grandchild の重みを gamma=3/2 で比較して選択).
//       (delta,gamma)=(5/2,3/2) が回転で平衡を回復することは test/.../verify_delta_gamma.py が z3 で検証済み.
//       ただし検証済みなのは merge_sub の文脈(回転相手が join 結果)だけなので, 単発の挿入/削除で
//       崩れた場合に自前で回転してはいけない(balance() が merge_sub に委ねる理由).
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
struct no_wl {}; // wl を持たない node の placeholder(空型).
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
  // clone(=木のコピー)は node をコピー構築する. Key の copy ctor が非 trivial だとエラーになるので弾く.
  static_assert(!ordered || is_trivially_copy_constructible_v<Key>, "Key must be trivially copy constructible");
  // key は代入で書き込む(new_leaf_key の lf.key, update の in.r.lo/sep). 代入できれば十分で trivial 性は不要.
  static_assert(!ordered || is_copy_assignable_v<Key>, "Key must be copy assignable");
  // node ctor は lf{} / in{{lc,rc},{}} で key/router を値初期化する. 既定構築できない Key は弾く.
  static_assert(!ordered || is_default_constructible_v<Key>, "Key must be default constructible");
  static_assert(!lazy && !beats, "lazy/beats not implemented yet (Phase 2/3)");

public:
  using ID = bbst::ID;
  // node ハンドル(pool の index 相当)= static_pool<node,N>::id_type. 通常ビルドでは uint32_t そのものだが,
  // LION_DEBUG では誤用検知のため世代付き構造体になる. 不透明な ID として扱い,
  // == 比較 / 代入 / pool への添字以外(算術・整数変換)には使わないこと.
  // node の定義より前に必要(node が子ハンドルを持つ)ため, pool<node> を実体化せずに
  // 参照できる T 非依存の別名 static_pool_id を使う(pool<node> の id_type と同じ型).
  using id_type = static_pool_id;
  // WL=true なら「左の子の重み」wl を持つ(index 降下で左の子の cache line を触らずに済む).
  // ただし wl を足すと node が大きくなるレイアウト(Value=int 系: 16→20B, 24→28B, 32→40B)では
  // key 降下が 10〜20% 遅くなり index 側の利得も消えるので, sizeof が変わらないとき(padding を吸収
  // できる Value=long long 系: 24/32/40B)だけ持つ. 判定は下の has_wl(sizeof 比較)で自動.
  template<bool WL>
  struct node_impl {
    Value val;
    int sz; // 部分木の葉数 = 重み. sz==1 ⟺ 葉.
    // 内部: 左の子の重み. 葉: 未使用(0). WL=false では空型(0 バイト).
    [[no_unique_address]] conditional_t<WL, int, wblt_impl::no_wl> wl;
    union {
      [[no_unique_address]] wblt_impl::leafkey<RawKey> lf;
      struct {
        id_type ch[2];
        [[no_unique_address]] wblt_impl::router<RawKey> r;
      } in;
    };
    // union のどのメンバも活性でない状態への代入(=生存期間未開始への書き込み)を避けるため,
    // 必ず mem-init で片方の variant member を構築する. static_pool::alloc が construct_at で呼ぶ.
    // 葉として構築. ordered の key は new_leaf_key が構築後に代入(lf は生存開始済みなので合法).
    explicit constexpr node_impl(Value const& v): val(v), sz(1), wl{}, lf{} {}
    // 内部として構築. val/sz/wl/router は直後の update() が埋める.
    constexpr node_impl(id_type lc, id_type rc): sz(0), wl{}, in{{lc, rc}, {}} {}
  };
  static constexpr bool has_wl = sizeof(node_impl<true>) == sizeof(node_impl<false>);
  using node = node_impl<has_wl>;

private:
  static constexpr id_type NIL = static_pool<node, N>::NIL;
  // 平衡パラメータ (delta, gamma) = (Dn/Dd, Gn/Gd) = (5/2, 3/2). 有理数なので交差乗算で比較(整数のみ).
  static constexpr int Dn = 5, Dd = 2, Gn = 3, Gd = 2;
  // 重み(sz)は int. like/single は交差乗算するため.
  static_assert(N <= size_t(INT_MAX) / Dn, "N too large: Dn*N must fit in int (weight comparison overflows)");
  // like(x,y): x <= delta*y ⟺ Dd*x <= Dn*y(x が y の delta 倍以下=平衡側). 否定が too_heavy.
  static constexpr bool like(int x, int y) { return Dd * x <= Dn * y; }
  // single(x,y): x <= gamma*y ⟺ Gd*x <= Gn*y(single rotation で足りる判定).
  static constexpr bool single(int x, int y) { return Gd * x <= Gn * y; }
  // DFS スタックの上界. 重み平衡なので高さ <= log_{(delta+1)/delta}(葉数) <= 約 2.06*log2(N). 余裕を見て確保.
  // 固定長ローカル array で使うので, static を持たず(破棄順問題なし)・ヒープ確保もゼロ.
  static constexpr int STACK_CAP = 3 * bit_width(N) + 4;
  static inline Value const e = Q_e();
  static inline static_pool<node, N> pool;
  id_type root;

  // ---- node 基本操作 ----
  static constexpr bool is_leaf(id_type i) { return pool[i].sz == 1; }
  static constexpr int w(id_type i) { return pool[i].sz; }
  static constexpr id_type child(id_type u, bool d) { return pool[u].in.ch[d]; }
  // 内部ノード u の左の子の重み. wl を持つ node ならそれを読む(左の子に触らない), 無ければ左の子の sz.
  static constexpr int wleft(id_type u) {
    if constexpr(has_wl) return pool[u].wl;
    else return w(child(u, 0));
  }
  static constexpr bool eqk(Key const& a, Key const& b) { return !comp(a, b) && !comp(b, a); }
  // 部分木の最小 key(ordered 時のみ).
  static constexpr Key const& lo(id_type i) { return is_leaf(i) ? pool[i].lf.key : pool[i].in.r.lo; }
  static constexpr id_type new_leaf(Value const& v) { return pool.alloc(v); } // node(Value) で葉を構築.
  static constexpr id_type new_leaf_key(Key const& k, Value const& v)
    requires(ordered)
  {
    id_type u = pool.alloc(v);
    pool[u].lf.key = k; // lf は node ctor で生存開始済み.
    return u;
  }
  // 集約を子から引き上げる. u は内部ノード.
  static constexpr void update(id_type u) {
    node& n = pool[u];
    id_type l = n.in.ch[0], r = n.in.ch[1];
    n.sz = pool[l].sz + pool[r].sz;
    if constexpr(has_wl) n.wl = pool[l].sz;
    n.val = Q(pool[l].val, pool[r].val);
    if constexpr(ordered) n.in.r.lo = lo(l), n.in.r.sep = lo(r);
  }
  // 集約だけを引き上げる(sz/router 据え置き). 値だけ書き換える pull-up 専用で,
  // 構造も key も変わらない update_sub/update_key/upsert_sub のヒット時に使う.
  static constexpr void update_val(id_type u) {
    node& n = pool[u];
    n.val = Q(pool[n.in.ch[0]].val, pool[n.in.ch[1]].val);
  }
  // l,r を子に持つ内部ノードを新規作成(平衡は呼び出し側が保証).
  static constexpr id_type make(id_type l, id_type r) {
    id_type u = pool.alloc(l, r); // node(id_type,id_type) で内部ノードを構築(in を活性化).
    update(u);
    return u;
  }

  // ---- 木操作コア: merge(join) / split / build / clone ----
  // l,r をこの順に連結(join). 片方が空なら他方. 平衡を保つ根を返す. O(logN).
  static constexpr id_type merge(id_type l, id_type r) {
    if(l == NIL) return r;
    if(r == NIL) return l;
    return merge_sub(l, r);
  }
  template<class... Args>
  static constexpr id_type merge(id_type a, Args... rest) { return merge(a, merge(rest...)); }
  static constexpr id_type merge_sub(id_type l, id_type r) { // l,r ともに非 NIL.
    if(!like(w(l), w(r))) {                                  // l が重すぎ(w(l)>delta*w(r)): l の右 spine を降下.
      id_type a = child(l, 0), c = merge_sub(child(l, 1), r);
      if(like(w(c), w(a))) {
        pool[l].in.ch[1] = c, update(l);
        return l;
      }
      if(single(w(child(c, 0)), w(child(c, 1)))) { // single rotation.
        pool[l].in.ch[1] = child(c, 0), update(l);
        pool[c].in.ch[0] = l, update(c);
        return c;
      }
      id_type cl = child(c, 0); // double rotation.
      pool[l].in.ch[1] = child(cl, 0);
      pool[c].in.ch[0] = child(cl, 1);
      update(l), update(c);
      pool[cl].in.ch[0] = l, pool[cl].in.ch[1] = c, update(cl);
      return cl;
    }
    if(!like(w(r), w(l))) { // r が重すぎ: 対称.
      id_type b = child(r, 1), c = merge_sub(l, child(r, 0));
      if(like(w(c), w(b))) {
        pool[r].in.ch[0] = c, update(r);
        return r;
      }
      if(single(w(child(c, 1)), w(child(c, 0)))) { // single rotation.
        pool[r].in.ch[0] = child(c, 1), update(r);
        pool[c].in.ch[1] = r, update(c);
        return c;
      }
      id_type cr = child(c, 1); // double rotation.
      pool[r].in.ch[0] = child(cr, 1);
      pool[c].in.ch[1] = child(cr, 0);
      update(r), update(c);
      pool[cr].in.ch[1] = r, pool[cr].in.ch[0] = c, update(cr);
      return cr;
    }
    return make(l, r);
  }
  // 子の一方だけが 1 要素分 増減した直後の内部ノード u を平衡化し, 新しい部分木の根を返す.
  // 崩れていない通常ケースは update だけ(pool の確保・解放ゼロ)で済ませる.
  // 崩れた場合だけ u を捨てて merge_sub に委ねる.
  static constexpr id_type balance(id_type u) {
    id_type l = pool[u].in.ch[0], r = pool[u].in.ch[1];
    if(like(w(l), w(r)) && like(w(r), w(l))) {
      update(u);
      return u;
    }
    pool.free(u);
    return merge_sub(l, r);
  }
  // u を先頭 k 個 / 残りに分ける(index split). u を消費して 2 根を返す. O(logN).
  static constexpr pair<id_type, id_type> split(id_type u, int k) {
    if(u == NIL) return {NIL, NIL};
    if(k <= 0) return {NIL, u};
    if(k >= w(u)) return {u, NIL};
    return split_sub(u, k);
  }
  static constexpr pair<id_type, id_type> split_sub(id_type u, int k) { // 0<k<w(u).
    id_type l = child(u, 0), r = child(u, 1);
    int wl = has_wl ? wleft(u) : w(l); // wl 無しでは読み込み済みの l を使う(u を読み直すと codegen が変わる).
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
  static constexpr id_type build(vector<Value> const& v, int l, int r) { // vector から平衡木を構築. O(N).
    if(l >= r) return NIL;
    if(l + 1 == r) return new_leaf(v[l]);
    int m = (l + r) / 2;
    return make(build(v, l, m), build(v, m, r));
  }
  static constexpr id_type build_key(vector<pair<Key, Value>> const& v, int l, int r)
    requires(ordered)
  {
    if(l >= r) return NIL;
    if(l + 1 == r) return new_leaf_key(v[l].first, v[l].second);
    int m = (l + r) / 2;
    return make(build_key(v, l, m), build_key(v, m, r));
  }
  // 部分木を丸ごと複製(deep copy). O(部分木サイズ).
  static constexpr id_type clone(id_type u) {
    if(u == NIL) return NIL;
    id_type v = pool.alloc(pool[u]); // node のコピー構築(union も活性メンバごと正しく複製).
    if(!is_leaf(u)) pool[v].in.ch[0] = clone(child(u, 0)), pool[v].in.ch[1] = clone(child(u, 1));
    return v;
  }
  static constexpr void free_all(id_type u) { // 部分木の全 node を free(固定長ローカル array で DFS).
    array<id_type, STACK_CAP> st;
    int top = 0;
    st[top++] = u;
    while(top) {
      id_type x = st[--top];
      if(!is_leaf(x)) assert(top + 2 <= STACK_CAP), st[top++] = child(x, 0), st[top++] = child(x, 1);
      pool.free(x);
    }
  }
  // 葉から順に f(葉ハンドル) を呼ぶ左→右 DFS(get_vec 系の共有).
  static constexpr void each_leaf(id_type u, auto const& f) {
    array<id_type, STACK_CAP> st;
    int top = 0;
    st[top++] = u;
    while(top) {
      id_type x = st[--top];
      if(is_leaf(x)) f(x);
      else assert(top + 2 <= STACK_CAP), st[top++] = child(x, 1), st[top++] = child(x, 0);
    }
  }

  // ---- モード横断の下請け(index を扱うコア) ----
  static constexpr id_type leaf_at(id_type u, int k) { // index k の葉ハンドル.
    while(!is_leaf(u)) {
      int wl = wleft(u);
      if(k < wl) u = child(u, 0);
      else k -= wl, u = child(u, 1);
    }
    return u;
  }
  // index k の葉まで降り, 通った内部ノードを path に積んで段数を返す(降下系の共有).
  // 戻り時に k は葉内の相対 index(0 or 負)に潰れている. u は降下後の葉になる.
  static constexpr int descend_at(id_type& u, int& k, id_type* path, bool* dir) {
    int d = 0;
    while(!is_leaf(u)) {
      int wl = wleft(u);
      bool b = k >= wl; // k==wl は右部分木の先頭.
      if(b) k -= wl;
      assert(d < STACK_CAP);
      path[d] = u, dir[d] = b, ++d, u = child(u, b);
    }
    return d;
  }
  // index k の値を g(値) に変更し祖先を pull-up(set は定数 g, modify は f). key 側 update_key と対.
  // 構造も key も動かないので巻き戻しは update_val だけ(sz/router の書き直しを省く).
  static constexpr void update_sub(id_type u, int k, auto const& g) {
    array<id_type, STACK_CAP> path;
    array<bool, STACK_CAP> dir;
    int d = descend_at(u, k, path.data(), dir.data());
    pool[u].val = g(pool[u].val);
    while(d--) update_val(path[d]);
  }
  // index k の位置に葉 lf を挿入する一降下(u は非 NIL). 各段は子ハンドルの差し替えと balance だけ. O(logN).
  static constexpr id_type insert_at(id_type u, int k, id_type lf) {
    array<id_type, STACK_CAP> path;
    array<bool, STACK_CAP> dir;
    int d = descend_at(u, k, path.data(), dir.data());
    id_type c = k <= 0 ? make(lf, u) : make(u, lf);
    while(d--) pool[path[d]].in.ch[dir[d]] = c, c = balance(path[d]);
    return c;
  }
  // index k の葉を削除する一降下. 平衡が保たれている段では確保も解放もしない. O(logN).
  // 「子が葉だったので畳む」のは最下段だけなので, 巻き戻しループの外に出す.
  static constexpr id_type erase_at(id_type u, int k) {
    array<id_type, STACK_CAP> path;
    array<bool, STACK_CAP> dir;
    int d = descend_at(u, k, path.data(), dir.data());
    pool.free(u);
    if(d == 0) return NIL; // 根が葉だった.
    --d;
    id_type c = child(path[d], !dir[d]); // free の前に兄弟を読む.
    pool.free(path[d]);
    while(d--) pool[path[d]].in.ch[dir[d]] = c, c = balance(path[d]);
    return c;
  }
  static constexpr Value prod_sub(id_type u, int l, int r) { // [l,r)∩[0,w(u)) の総積(非空前提).
    if(l <= 0 && r >= w(u)) return pool[u].val;
    id_type a = child(u, 0), b = child(u, 1);
    int wl = has_wl ? wleft(u) : w(a); // 同上.
    if(r <= wl) return prod_sub(a, l, r);
    if(l >= wl) return prod_sub(b, l - wl, r - wl);
    return Q(prod_sub(a, l, wl), prod_sub(b, 0, r - wl));
  }
  // u の先頭から f を保つ最大の右端 r を返す(木上二分探索, 確保なしの top-down). acc は流入積.
  static constexpr int mr(id_type u, int l, Value& acc, auto const& f) {
    if(l <= 0) {
      Value na = Q(acc, pool[u].val);
      if(f(na)) {
        acc = na;
        return w(u);
      }
    }
    if(is_leaf(u)) return 0;
    int wl = wleft(u);
    if(l >= wl) return wl + mr(child(u, 1), l - wl, acc, f);
    int lc = mr(child(u, 0), l, acc, f);
    if(lc < wl) return lc;
    return wl + mr(child(u, 1), 0, acc, f);
  }
  // u の [0,r) 内で f を保つ最小の左端 l を返す. acc は流入積.
  static constexpr int ml(id_type u, int r, Value& acc, auto const& f) {
    if(r >= w(u)) {
      Value na = Q(pool[u].val, acc);
      if(f(na)) {
        acc = na;
        return 0;
      }
    }
    if(is_leaf(u)) return 1;
    int wl = wleft(u);
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
    id_type u = root;
    int idx = 0;
    while(!is_leaf(u)) {
      Key const& sp = pool[u].in.r.sep;
      bool go_left = le ? comp(k, sp) : !comp(sp, k);
      if(go_left) u = child(u, 0);
      else idx += wleft(u), u = child(u, 1);
    }
    Key const& lk = pool[u].lf.key;
    if(le ? !comp(k, lk) : comp(lk, k)) ++idx;
    return idx;
  }
  // key=k が入るべき葉まで降り, 通った内部ノードを path に積んで段数を返す(更新系降下の共有).
  // find_leaf と同じ降下だが, 巻き戻しに使う経路と方向を記録する点だけが違う.
  static constexpr int descend_key(id_type& u, Key const& k, id_type* path, bool* dir) {
    int d = 0;
    while(!is_leaf(u)) {
      bool b = !comp(k, pool[u].in.r.sep);
      assert(d < STACK_CAP);
      path[d] = u, dir[d] = b, ++d, u = child(u, b);
    }
    return d;
  }
  // key=k が入るべき葉を 1 降下で返す(exact search). key 一意なので唯一.
  // index は返さない: index を数えるには右へ降りるたび左の子の重みを読む必要があり,
  // 参照系(contains/get/get_or)では触らずに済む cache line を無駄に触ることになるため.
  static constexpr id_type find_leaf(id_type u, Key const& k) {
    while(!is_leaf(u)) u = child(u, !comp(k, pool[u].in.r.sep));
    return u;
  }
  // key=k の葉を探す一降下. 見つかれば on_hit(葉) を呼び木構造は変えない(祖先の集約だけ更新),
  // 無ければ make_leaf() が返す葉をその位置に挿入して平衡を保つ. u は非 NIL. O(logN).
  // upsert / modify_or / insert_leaf の共通実装.
  static constexpr id_type upsert_sub(id_type u, Key const& k, auto const& on_hit, auto const& make_leaf) {
    array<id_type, STACK_CAP> path;
    array<bool, STACK_CAP> dir;
    int d = descend_key(u, k, path.data(), dir.data());
    Key const& lk = pool[u].lf.key;
    bool klt = comp(k, lk), lgt = comp(lk, k);
    if(!klt && !lgt) { // key 一致: 葉をその場で書き換えるので, 引数が この葉を指していても安全.
      on_hit(u);       // 構造が変わらないので balance は不要, 集約だけ引き上げる.
      for(int i = d; i--;) update_val(path[i]);
      return d ? path[0] : u;
    }
    id_type c = klt ? make(make_leaf(), u) : make(u, make_leaf());
    while(d--) pool[path[d]].in.ch[dir[d]] = c, c = balance(path[d]);
    return c;
  }
  // key=k の葉を削除する一降下. 見つかれば ok=true にして削除後の根を返す. 無ければ u をそのまま返す.
  static constexpr id_type erase_key(id_type u, Key const& k, bool& ok) {
    array<id_type, STACK_CAP> path;
    array<bool, STACK_CAP> dir;
    id_type r0 = u;
    int d = descend_key(u, k, path.data(), dir.data());
    if(!eqk(k, pool[u].lf.key)) return r0; // 無かった: 木は一切変えていないので元の根を返す.
    ok = true, pool.free(u);
    if(d == 0) return NIL; // 根が葉だった.
    --d;
    id_type c = child(path[d], !dir[d]); // free の前に兄弟を読む.
    pool.free(path[d]);
    while(d--) pool[path[d]].in.ch[dir[d]] = c, c = balance(path[d]);
    return c;
  }
  // key=k の葉の val を g(val) に変更(1 降下で祖先 pull-up). 見つかったか返す(set/modify 共有).
  // 構造も key も動かないので巻き戻しは update_val だけ(sz/router の書き直しを省く).
  static constexpr bool update_key(id_type u, Key const& k, auto const& g) {
    array<id_type, STACK_CAP> path;
    array<bool, STACK_CAP> dir;
    int d = descend_key(u, k, path.data(), dir.data());
    if(!eqk(k, pool[u].lf.key)) return false;
    pool[u].val = g(pool[u].val);
    while(d--) update_val(path[d]);
    return true;
  }
  // u 内の key>=kl の接尾辞積(左端固定の反復降下). 左へ降りるたび右の子を丸ごと前置する.
  static constexpr Value suffix_key(id_type u, Key const& kl) {
    Value acc = e;
    while(!is_leaf(u)) {
      if(!comp(kl, pool[u].in.r.sep)) u = child(u, 1); // sep<=kl: 左は全て範囲外.
      else acc = Q(pool[child(u, 1)].val, acc), u = child(u, 0);
    }
    return comp(pool[u].lf.key, kl) ? acc : Q(pool[u].val, acc);
  }
  // u 内の key<kr の接頭辞積(右端固定の反復降下). 右へ降りるたび左の子を丸ごと後置する.
  static constexpr Value prefix_key(id_type u, Key const& kr) {
    Value acc = e;
    while(!is_leaf(u)) {
      if(!comp(pool[u].in.r.sep, kr)) u = child(u, 0); // sep>=kr: 右は全て範囲外.
      else acc = Q(acc, pool[child(u, 0)].val), u = child(u, 1);
    }
    return comp(pool[u].lf.key, kr) ? Q(acc, pool[u].val) : acc;
  }
  // key ∈ [kl,kr) の総積. 分岐ノードまで降りてから左右 1 本ずつ(index 版 prod_sub と同じ構造).
  // 前提 kl<kr かつ u 非 NIL. 積の順序は Q(左の接尾辞, 右の接頭辞) で左→右が保たれる.
  static constexpr Value prod_key(id_type u, Key const& kl, Key const& kr) {
    while(!is_leaf(u)) {
      Key const& sp = pool[u].in.r.sep;
      if(!comp(kl, sp)) u = child(u, 1);                                       // sep<=kl: 範囲は全て右部分木.
      else if(!comp(sp, kr)) u = child(u, 0);                                  // sep>=kr: 範囲は全て左部分木.
      else return Q(suffix_key(child(u, 0), kl), prefix_key(child(u, 1), kr)); // 分岐点.
    }
    Key const& lk = pool[u].lf.key; // 分岐せず葉に着いた: 単独で範囲判定.
    return !comp(lk, kl) && comp(lk, kr) ? pool[u].val : e;
  }
  // key>=kl を左端に固定した木上二分探索(一降下). 戻り値は u 内での index. acc は流入積.
  // mr(index 版)と同じ形だが「u の key が全て kl 以上か」を router lo で O(1) 判定するので,
  // lower_bound を別に走らせる必要がない. 戻り値が w(u) なら「u を全部使い切った(継続可)」.
  static constexpr int mr_key(id_type u, Key const& kl, Value& acc, auto const& f) {
    if(!comp(lo(u), kl)) { // u の key は全て範囲内.
      Value na = Q(acc, pool[u].val);
      if(f(na)) {
        acc = na;
        return w(u);
      }
      if(is_leaf(u)) return 0;
    } else if(is_leaf(u)) return 1; // key<kl: 範囲より前なので丸ごと読み飛ばす.
    int wl = wleft(u);
    // sep<=kl なら左部分木の key は全て kl 未満(左は丸ごと範囲前).
    if(!comp(kl, pool[u].in.r.sep)) return wl + mr_key(child(u, 1), kl, acc, f);
    int lc = mr_key(child(u, 0), kl, acc, f);
    if(lc < wl) return lc;
    return wl + mr_key(child(u, 1), kl, acc, f);
  }
  // key<kr を右端に固定した木上二分探索(一降下). khi=u の key の strict upper(nullptr=+inf).
  // 呼び出し時, u の最小 key は kr 未満であること(=範囲が非空)を保証する.
  static constexpr int ml_key(id_type u, Key const& kr, Value& acc, auto const& f, Key const* khi) {
    // u が丸ごと範囲内(全 key<kr)か. 葉は key を直接見る, 内部は親から渡された上界で判定する.
    if(is_leaf(u) ? comp(pool[u].lf.key, kr) : (khi && !comp(kr, *khi))) {
      Value na = Q(pool[u].val, acc);
      if(f(na)) {
        acc = na;
        return 0;
      }
    }
    if(is_leaf(u)) return 1;
    int wl = wleft(u);
    Key const& sp = pool[u].in.r.sep;
    if(!comp(sp, kr)) return ml_key(child(u, 0), kr, acc, f, &sp); // sep>=kr: 右は全て範囲外.
    int rc = ml_key(child(u, 1), kr, acc, f, khi);
    if(rc > 0) return wl + rc;
    return ml_key(child(u, 0), kr, acc, f, &sp);
  }
  static constexpr pair<id_type, id_type> split_key(id_type u, Key const& s) { // key<s | key>=s.
    if(u == NIL) return {NIL, NIL};
    // 部分木の最小 key が s 以上なら丸ごと右. 葉の「右へ」もここが兼ねる.
    if(!comp(lo(u), s)) return {NIL, u};
    if(is_leaf(u)) return {u, NIL}; // 葉で lo(u)<s: 丸ごと左.
    id_type l = child(u, 0), r = child(u, 1);
    Key sp = pool[u].in.r.sep; // free 後に使うのでコピー必須.
    pool.free(u);
    if(!comp(sp, s)) { // sp>=s: 境界は左.
      auto [a, b] = split_key(l, s);
      return {a, merge(b, r)};
    }
    auto [a, b] = split_key(r, s);
    return {merge(l, a), b};
  }
  // 葉 lf(単一要素)を map t に統合挿入(一降下). key 衝突時は既存葉に f(lf.val, 既存.val) を書いて
  // lf を解放する. O(logN).
  static constexpr id_type insert_leaf(id_type t, id_type lf, auto const& f) {
    if(t == NIL) return lf;
    Key k = pool[lf].lf.key; // 衝突時に lf を解放するので key はコピーで持つ.
    auto hit = [&](id_type u) { pool[u].val = f(pool[lf].val, pool[u].val), pool.free(lf); };
    return upsert_sub(t, k, hit, [&] { return lf; });
  }
  // key 一意の map として union. どちらかが葉ならそれを他方へ挿入(高速化). 両内部なら大きい方で split.
  static constexpr id_type meld_sub(id_type a, id_type b, auto const& f) {
    if(a == NIL) return b;
    if(b == NIL) return a;
    if(is_leaf(a)) return insert_leaf(b, a, f);
    if(is_leaf(b)) return insert_leaf(a, b, f);
    if(w(a) < w(b)) swap(a, b); // 大きい方 a を分割軸に(小さい方 b を回す).
    Key s = pool[a].in.r.sep;
    id_type al = child(a, 0), ar = child(a, 1);
    pool.free(a);
    auto [bl, br] = split_key(b, s);
    return merge(meld_sub(al, bl, f), meld_sub(ar, br, f));
  }
  static constexpr int height_sub(id_type u) {
    return is_leaf(u) ? 1 : 1 + max(height_sub(child(u, 0)), height_sub(child(u, 1)));
  }
  // 部分木の {min_key, max_key}(ordered のみ意味あり)を返しつつ全不変条件を assert.
  // クラステンプレートのメンバなので validate() から呼ばれない限り実体化されない(未使用でも 0 コスト).
  static pair<Key, Key> validate_sub(id_type u) {
    if(is_leaf(u)) {
      assert(w(u) == 1 && "leaf weight must be 1");
      if constexpr(ordered) return {pool[u].lf.key, pool[u].lf.key};
      else return {};
    }
    id_type l = child(u, 0), r = child(u, 1);
    pair<Key, Key> pl = validate_sub(l), pr = validate_sub(r);
    assert(w(u) == w(l) + w(r) && "sz != child sz sum");
    if constexpr(has_wl) assert(pool[u].wl == w(l) && "wl != left child weight");
    assert(like(w(l), w(r)) && like(w(r), w(l)) && "weight-balance violated");
    if constexpr(equality_comparable<Value>)
      assert(pool[u].val == Q(pool[l].val, pool[r].val) && "val != Q(children)");
    if constexpr(ordered) {
      assert(comp(pl.second, pr.first) && "keys not strictly increasing (unique map)");
      assert(eqk(pool[u].in.r.lo, pl.first) && "router lo != subtree min key");
      assert(eqk(pool[u].in.r.sep, pr.first) && "router sep != right subtree min key");
      return {pl.first, pr.second};
    } else {
      (void)pl, (void)pr;
      return {};
    }
  }

  // 根ハンドルを受け取って所有する private ctor のタグ(split_off の返り値用).
  // tag を挟むのは, 1 引数 ctor にすると id_type から木への変換候補ができてしまい,
  // 借り物の部分木ハンドル(child()/merge()/split() の戻り)が黙って「所有する木」に化けるため.
  // 加えて id_type は通常ビルドで uint32_t, LION_DEBUG では構造体なので,
  // 1 引数 ctor だとそのミスが通常ビルドでだけコンパイルできてしまう(検出がビルド依存になる).
  struct from_root_t {};
  constexpr weight_balanced_leafy_tree(from_root_t, id_type r): root(r) {}

public:
  // static_pool::fast への参照. true で自動破棄を省略(=生存要素の Value dtor を捨てる高速化).
  static inline bool& fast = static_pool<node, N>::fast;

  constexpr weight_balanced_leafy_tree(): root(NIL) {}
  constexpr weight_balanced_leafy_tree(vector<Value> const& v)
    requires(!ordered)
      : root(build(v, 0, v.size())) { validate(); }
  // sorted でなければ stable_sort. key は一意でなければ assert(map 意味論).
  constexpr weight_balanced_leafy_tree(vector<pair<Key, Value>> v)
    requires(ordered)
      : root(NIL) {
    stable_sort(v.begin(), v.end(), [](auto const& x, auto const& y) { return comp(x.first, y.first); });
    for(size_t i = 1; i < v.size(); ++i) assert(!eqk(v[i - 1].first, v[i].first) && "duplicate keys not allowed");
    root = build_key(v, 0, v.size());
    validate();
  }
  // copy は deep copy(node を共有しない). O(N).
  constexpr weight_balanced_leafy_tree(weight_balanced_leafy_tree const& ot): root(clone(ot.root)) { validate(); }
  constexpr weight_balanced_leafy_tree& operator=(weight_balanced_leafy_tree const& ot) {
    if(this != &ot) {
      if(!fast) clear();
      root = clone(ot.root);
      validate();
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
  // LION_DEBUG 設定時のみ, 現在の木が全不変条件(sz/集約/重み平衡/key 順序/router lo,sep)を
  // 満たすか検査(assert). 非設定時は no-op. O(N).
  constexpr void validate() const {
#ifdef LION_DEBUG
    if(root != NIL) validate_sub(root);
#endif
  }

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
    validate();
  }
  // O(logN) index k の値を f(値) に置換(名前 apply は遅延の区間作用に予約).
  template<class F>
    requires(!ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify(int k, F const& f) {
    assert(0 <= k && k < size());
    update_sub(root, k, f);
    validate();
  }
  // O(logN) index k の手前に v を挿入(挿入後に k 番目).
  constexpr void insert(int k, Value const& v)
    requires(!ordered)
  {
    assert(0 <= k && k <= size());
    id_type lf = new_leaf(v);
    root = root == NIL ? lf : insert_at(root, k, lf);
    validate();
  }
  // O(logN) index k の要素を削除.
  constexpr void erase(int k)
    requires(!ordered)
  {
    assert(0 <= k && k < size());
    root = erase_at(root, k);
    validate();
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
    if(root != NIL) each_leaf(root, [&](id_type u) { ret.push_back(pool[u].val); });
    return ret;
  }

  // ===== 列の分割・連結(sequence). merge/split のコアをそのまま公開する. =====
  // O(logN) 先頭 k 個を this に残し, index k 以降を新しい木として返す.
  constexpr weight_balanced_leafy_tree split_off(int k)
    requires(!ordered)
  {
    assert(0 <= k && k <= size());
    auto [l, r] = split(root, k);
    root = l;
    validate();
    weight_balanced_leafy_tree ret(from_root_t{}, r);
    ret.validate();
    return ret;
  }
  // O(logN) ot をこの木の末尾に連結(ot は空になる).
  constexpr void concat(weight_balanced_leafy_tree& ot)
    requires(!ordered)
  {
    assert(this != &ot);
    root = merge(root, ot.root), ot.root = NIL;
    validate();
  }
  // O(logN) ot を index k の手前に丸ごと差し込む(ot は空になる).
  constexpr void insert(int k, weight_balanced_leafy_tree& ot)
    requires(!ordered)
  {
    assert(0 <= k && k <= size() && this != &ot);
    if(ot.root == NIL) return;
    auto [a, b] = split(root, k);
    root = merge(a, ot.root, b); // 可変長 merge.
    ot.root = NIL;
    validate();
  }
  // O((r-l) + logN) index [l,r) をまとめて削除.
  // 内訳は split/merge が O(logN), 切り出した部分木の node を pool に返す free_all が O(r-l).
  constexpr void erase(int l, int r)
    requires(!ordered)
  {
    assert(0 <= l && l <= r && r <= size());
    if(l >= r) return;
    auto [a, bc] = split(root, l);
    auto [b, c] = split(bc, r - l);
    if(b != NIL) free_all(b);
    root = merge(a, c);
    validate();
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
  { return root != NIL && eqk(k, pool[find_leaf(root, k)].lf.key); }
  // O(logN) key=k の値. 存在しなければ assert 失敗.
  constexpr Value const& get(Key const& k) const
    requires(ordered)
  {
    assert(root != NIL);
    id_type u = find_leaf(root, k);
    assert(eqk(k, pool[u].lf.key) && "get(Key): key not found");
    return pool[u].val;
  }
  // O(logN) key=k の値. 無ければ def(既定 e)を返す.
  constexpr Value get_or(Key const& k, Value const& def = e) const
    requires(ordered)
  {
    if(root == NIL) return def;
    id_type u = find_leaf(root, k);
    return eqk(k, pool[u].lf.key) ? pool[u].val : def;
  }
  // O(logN) key=k の値を v に. 存在しなければ assert 失敗.
  constexpr void set(Key const& k, Value const& v)
    requires(ordered)
  {
    bool ok = root != NIL && update_key(root, k, [&](Value const&) { return v; });
    assert(ok && "set(Key,Value): key not found");
    (void)ok;
    validate();
  }
  // O(logN) key=k の値を f(値) に置換. 存在しなければ assert 失敗(名前 apply は遅延の区間作用に予約).
  template<class F>
    requires(ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify(Key const& k, F const& f) {
    bool ok = root != NIL && update_key(root, k, f);
    assert(ok && "modify(Key,f): key not found");
    (void)ok;
    validate();
  }
  // O(logN) key=k があれば f(値) に置換, 無ければ f(e) を挿入(upsert). 存在時はその場更新で速い.
  template<class F>
    requires(ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify_or(Key const& k, F const& f) {
    auto hit = [&](id_type u) { pool[u].val = f(pool[u].val); };
    auto mk = [&] { return new_leaf_key(k, f(e)); };
    root = root == NIL ? mk() : upsert_sub(root, k, hit, mk);
    validate();
  }
  // O(logN) key=k を挿入. 既に存在すれば値を v に置換(update-or-insert なので名前は upsert).
  // 一降下で既存葉をその場更新する.
  constexpr void upsert(Key const& k, Value const& v)
    requires(ordered)
  {
    auto hit = [&](id_type u) { pool[u].val = v; };
    auto mk = [&] { return new_leaf_key(k, v); };
    root = root == NIL ? mk() : upsert_sub(root, k, hit, mk);
    validate();
  }
  // O(logN) key=k を削除. 存在したかを返す. 探索と削除を 1 本にまとめた一降下.
  constexpr bool erase(Key const& k)
    requires(ordered)
  {
    if(root == NIL) return false;
    bool erased = false;
    root = erase_key(root, k, erased);
    validate();
    return erased;
  }
  // O(logN) key>=s の要素を切り離して新しい木として返す(this には key<s だけが残る).
  constexpr weight_balanced_leafy_tree split_off(Key const& s)
    requires(ordered)
  {
    auto [l, r] = split_key(root, s);
    root = l;
    validate();
    weight_balanced_leafy_tree ret(from_root_t{}, r);
    ret.validate();
    return ret;
  }
  // O(logN) key ∈ [kl,kr) の総積(1 降下).
  constexpr Value prod(Key const& kl, Key const& kr) const
    requires(ordered)
  {
    if(root == NIL || !comp(kl, kr)) return e;
    return prod_key(root, kl, kr);
  }
  // O(logN) key=kl 以上を左端に固定した木上二分探索. 返り値は index.
  // lower_bound の降下と二分探索の降下を融合した一降下(左端判定は router lo で O(1)).
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int max_right(F const& f, Key const& kl) const {
    assert(f(e));
    if(root == NIL) return 0;
    Value acc = e;
    return mr_key(root, kl, acc, f);
  }
  // O(logN) key=kr 未満を右端に固定した木上二分探索. 返り値は index. 同じく一降下.
  template<class F>
    requires(ordered && is_invocable_r_v<bool, F, Value>)
  constexpr int min_left(F const& f, Key const& kr) const {
    assert(f(e));
    if(root == NIL || !comp(lo(root), kr)) return 0; // 全 key が kr 以上: 範囲が空.
    Value acc = e;
    return ml_key(root, kr, acc, f, nullptr);
  }
  // O(M log(1+N/M)) ot を this に union. key 衝突は f で統合(内部で small-to-large するため
  // 引数順は不定 → f は可換推奨. 既定 Q). ot は空に. f は Value(Value,Value).
  template<class F = decltype(Q)>
    requires(ordered && is_invocable_r_v<Value, F, Value, Value>)
  constexpr void meld(weight_balanced_leafy_tree& ot, F const& f = Q) {
    if(this == &ot) return;
    root = meld_sub(root, ot.root, f);
    ot.root = NIL;
    validate();
  }
  // O(N) 中身は pool 内 node への参照(木を変更するまで有効).
  constexpr vector<pair<Key const&, Value const&>> get_vec_pair() const
    requires(ordered)
  {
    vector<pair<Key const&, Value const&>> ret;
    ret.reserve(size());
    if(root != NIL) each_leaf(root, [&](id_type u) { ret.emplace_back(pool[u].lf.key, pool[u].val); });
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
    validate();
  }
  // O(logN) index i の値を f(値) に置換(名前 apply は遅延の区間作用に予約).
  template<class F>
    requires(ordered && is_invocable_r_v<Value, F, Value>)
  constexpr void modify(ID i, F const& f) {
    assert(0 <= i.id && i.id < size());
    update_sub(root, i.id, f);
    validate();
  }
  // O(logN) index i の要素を削除.
  constexpr void erase(ID i)
    requires(ordered)
  {
    assert(0 <= i.id && i.id < size());
    root = erase_at(root, i.id);
    validate();
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
