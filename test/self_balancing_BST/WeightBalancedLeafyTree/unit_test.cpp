// WeightBalancedLeafyTree ユニットテスト.
// 非可換モノイド(アフィン合成 x -> a*x+b)で prod の適用順序と全 API を検証する.
// compile: g++ -std=c++23 -O2 -D__LION_DEBUG -I<repo>/cpp_library unit_test.cpp
//   __LION_DEBUG を付けると各変更操作の内部で validate() が全不変条件を assert する(1 操作 O(N)).
// run: ./unit_test [seed]   (seed 省略時は既定値)
#include "lion/WeightBalancedLeafyTree.hpp"
#include <bits/stdc++.h>
using namespace std;
using ll = long long;

// ===== 非可換モノイド: アフィン合成 =====
constexpr ll MOD = 998244353;
using Af = array<ll, 2>; // {a,b}: x -> a*x+b.
// 左から順に適用: af_comp(f,g) は「f を適用してから g」= g(f(x)) = a2*(a1 x + b1) + b2.
constexpr Af af_comp(Af f, Af g) { return {f[0] * g[0] % MOD, (g[0] * f[1] + g[1]) % MOD}; }
constexpr Af af_id() { return {1, 0}; }
ll apply_af(Af f, ll x) { return (f[0] * x + f[1]) % MOD; }
// 木上二分探索用の非可換モノイド: MOD を取らないアフィン合成.
// 要素を a>=1, b>=0 に限ると F(0)=b は区間の伸長(左右どちら向きでも)に対し単調非減少になるので,
// f(F) = (F(0)<=thr) が単調述語として使える. 和モノイドと違い acc の左右取り違えを検出できる.
constexpr Af afn_comp(Af f, Af g) { return {f[0] * g[0], g[0] * f[1] + g[1]}; }

using Seq = weight_balanced_leafy_tree<Mode::plain, void, Af, af_comp, af_id>;   // 列(非可換).
using Ord = weight_balanced_leafy_tree<Mode::plain, int, Af, af_comp, af_id>;    // map(非可換).
using SeqN = weight_balanced_leafy_tree<Mode::plain, void, Af, afn_comp, af_id>; // 木上二分探索(列, 非可換).
using OrdN = weight_balanced_leafy_tree<Mode::plain, int, Af, afn_comp, af_id>;  // 木上二分探索(map, 非可換).
// Value=ll の素朴な実体. レイアウト検査と平衡テストで使う.
using SeqS = weight_balanced_leafy_tree<Mode::plain, void, ll>;
using OrdS = weight_balanced_leafy_tree<Mode::plain, int, ll>;

static_assert(!Seq::ordered && Ord::ordered);
// Key=void なら leafkey/router が [[no_unique_address]] で 0 バイトに畳まれる(union は子 2 本の 8B のみ).
static_assert(sizeof(SeqS::node) == 24 && sizeof(OrdS::node) == 32);
static_assert(is_trivially_destructible_v<SeqS::node> && is_trivially_destructible_v<OrdS::node>);

int Fc = 0;
auto fail = [](char const* m) { cerr << "FAIL: " << m << "\n"; ++Fc; };
Af rnd_af(mt19937& rng) { return {(ll)(rng() % MOD), (ll)(rng() % MOD)}; }

// prod([l,r)) が左→右の合成であること(可換化バグを検出).
void order_test() {
  vector<Af> v = {{2, 3}, {5, 7}, {11, 13}, {17, 19}};
  Seq t(v);
  Af got = t.prod(0, 4), want = af_id(), rev = af_id();
  for(auto& f: v) want = af_comp(want, f);
  for(int i = 3; i >= 0; --i) rev = af_comp(rev, v[i]);
  if(got != want) fail("prod is not left-to-right composition");
  if(want == rev) fail("test monoid is accidentally commutative (weaken test)");
  ll x0 = 12345, expx = x0;
  for(auto& f: v) expx = apply_af(f, expx);
  if(apply_af(got, x0) != expx) fail("applied value mismatch");
}

void seq_test(mt19937& rng) {
  for(int iter = 0; iter < 1500 && Fc <= 10; ++iter) {
    Seq t;
    vector<Af> a;
    int ops = 1 + rng() % 50;
    for(int o = 0; o < ops; ++o) {
      int type = rng() % 6, n = a.size();
      if(type == 0) {
        int k = rng() % (n + 1);
        Af v = rnd_af(rng);
        t.insert(k, v);
        a.insert(a.begin() + k, v);
      } else if(type == 1 && n) {
        int k = rng() % n;
        t.erase(k);
        a.erase(a.begin() + k);
      } else if(type == 2 && n) {
        int k = rng() % n;
        Af v = rnd_af(rng);
        t.set(k, v);
        a[k] = v;
      } else if(type == 3 && n) {
        int k = rng() % n;
        t.modify(k, [](Af x) { return af_comp(x, {2, 1}); });
        a[k] = af_comp(a[k], {2, 1});
      } else if(type == 4 && n) {
        int l = rng() % (n + 1), r = rng() % (n + 1);
        if(l > r) swap(l, r);
        Af got = t.prod(l, r), want = af_id();
        for(int i = l; i < r; ++i) want = af_comp(want, a[i]);
        if(got != want) fail("seq prod (non-commutative order)");
        if(t.get(l % max(n, 1)) != a[l % max(n, 1)]) fail("seq get");
      } else {
        if(t.get_vec() != a || t.size() != (int)a.size()) fail("seq get_vec/size");
        Af want = af_id();
        for(auto& f: a) want = af_comp(want, f);
        if(t.all_prod() != want) fail("seq all_prod");
        if(t.prod(0, 0) != af_id() || t.prod(n, n) != af_id()) fail("seq empty-range prod");
      }
    }
    t.clear();
    // 空木: size/prod/all_prod/get_vec が単位元・空を返す.
    if(!t.empty() || t.size() != 0) fail("seq clear");
    if(t.prod(0, 0) != af_id() || t.all_prod() != af_id() || !t.get_vec().empty()) fail("seq empty tree");
  }
}

// ordered の全 key/ID API を map と照合.
void ord_test(mt19937& rng) {
  for(int iter = 0; iter < 1500 && Fc <= 10; ++iter) {
    Ord t;
    map<int, Af> mp;
    int ops = 1 + rng() % 60;
    for(int o = 0; o < ops; ++o) {
      int type = rng() % 13, k = rng() % 40;
      auto idx_of = [&](int key) { return (int)distance(mp.begin(), mp.lower_bound(key)); };
      if(type == 0) {
        Af v = rnd_af(rng);
        t.upsert(k, v);
        mp[k] = v;
      } else if(type == 1) {
        if(t.erase(k) != (bool)mp.count(k)) fail("erase ret");
        mp.erase(k);
      } else if(type == 2) {
        if(t.lower_bound(k) != idx_of(k)) fail("lower_bound");
      } else if(type == 3) {
        if(t.upper_bound(k) != (int)distance(mp.begin(), mp.upper_bound(k))) fail("upper_bound");
      } else if(type == 4) {
        if(t.contains(k) != (bool)mp.count(k)) fail("contains");
      } else if(type == 5) {
        Af def = {7, 7};
        Af got = t.get_or(k, def), want = mp.count(k) ? mp[k] : def;
        if(got != want) fail("get_or");
        if(mp.count(k) && t.get(k) != mp[k]) fail("get(Key)");
      } else if(type == 6) {
        if(mp.count(k)) {
          Af v = rnd_af(rng);
          t.set(k, v);
          mp[k] = v;
        }
      } else if(type == 7) {
        if(mp.count(k)) {
          t.modify(k, [](Af x) { return af_comp({3, 4}, x); });
          mp[k] = af_comp({3, 4}, mp[k]);
        }
      } else if(type == 8) { // modify_or: 存在なら f(v), 不在なら f(e) を挿入.
        auto f = [](Af x) { return af_comp(x, {5, 6}); };
        t.modify_or(k, f);
        mp[k] = f(mp.count(k) ? mp[k] : af_id());
      } else if(type == 9) {
        int kl = rng() % 40, kr = rng() % 40;
        if(kl > kr) swap(kl, kr);
        Af got = t.prod(kl, kr), want = af_id();
        for(auto& [kk, v]: mp)
          if(kl <= kk && kk < kr) want = af_comp(want, v);
        if(got != want) fail("prod(Key,Key) order");
      } else if(type == 10 && !mp.empty()) { // ID API: get/key_at/prod(ID,ID)/set(ID)/modify(ID)
        vector<pair<int, Af>> aa(mp.begin(), mp.end());
        int i = rng() % aa.size();
        if(t.get(Ord::ID(i)) != aa[i].second) fail("get(ID)");
        if(t.key_at(i) != aa[i].first) fail("key_at");
        int l = rng() % (aa.size() + 1), r = rng() % (aa.size() + 1);
        if(l > r) swap(l, r);
        Af got = t.prod(Ord::ID(l), Ord::ID(r)), want = af_id();
        for(int j = l; j < r; ++j) want = af_comp(want, aa[j].second);
        if(got != want) fail("prod(ID,ID) order");
        Af nv = rnd_af(rng);
        t.set(Ord::ID(i), nv);
        mp[aa[i].first] = nv;
        t.modify(Ord::ID(i), [](Af x) { return af_comp(x, {9, 1}); });
        mp[aa[i].first] = af_comp(nv, {9, 1});
      } else if(type == 11 && !mp.empty()) { // erase(ID): index 指定削除. 消えた key を照合して off-by-one を検出.
        int i = rng() % mp.size();
        auto it = next(mp.begin(), i);
        int ek = it->first;
        t.erase(Ord::ID(i));
        mp.erase(it);
        if(t.contains(ek) || t.size() != (int)mp.size()) fail("erase(ID)");
      } else {
        Af want = af_id();
        for(auto& [kk, v]: mp) want = af_comp(want, v);
        if(t.all_prod() != want) fail("all_prod order");
        vector<pair<int, Af>> wv(mp.begin(), mp.end()), gv;
        for(auto [kk, vv]: t.get_vec_pair()) gv.emplace_back(kk, vv);
        if(gv != wv || t.size() != (int)mp.size()) fail("get_vec_pair/size");
      }
    }
    t.clear();
  }
}

// 木上二分探索. sequence の index 版と ordered の ID/Key 版を同じ ground truth に照合する.
// 非可換モノイドで回すのが要点: 和モノイドだと mr の Q(acc,val) と ml の Q(val,acc) を
// 取り違えても結果が変わらず素通りしてしまう.
void binsearch_test(mt19937& rng) {
  for(int iter = 0; iter < 1500 && Fc <= 10; ++iter) {
    int n = rng() % 40;
    vector<Af> a(n);
    for(auto& x: a) x = {(ll)(1 + rng() % 2), (ll)(rng() % 4)}; // a>=1,b>=0 → 伸長で F(0) 単調非減少.
    SeqN t(a);
    OrdN o;
    for(int i = 0; i < n; ++i) o.upsert(i * 2, a[i]); // key = 0,2,4,... (index i の key は 2i).
    ll thr = rng() % 2000;
    auto f = [&](Af F) { return F[1] <= thr; }; // F(0)=b. f(単位元)=(0<=thr)=true.
    // 期待値: [l,·) を右へ伸ばせる最大の r / [·,r) を左へ伸ばせる最小の l.
    auto want_mr = [&](int l) {
      int ex = l;
      Af acc = af_id();
      for(int r = l; r < n; ++r) {
        acc = afn_comp(acc, a[r]);
        if(f(acc)) ex = r + 1;
        else break;
      }
      return ex;
    };
    auto want_ml = [&](int r) {
      int ex = r;
      Af acc = af_id();
      for(int i = r - 1; i >= 0; --i) {
        acc = afn_comp(a[i], acc);
        if(f(acc)) ex = i;
        else break;
      }
      return ex;
    };
    for(int q = 0; q < 3; ++q) {
      int l = rng() % (n + 1), r = rng() % (n + 1);
      if(t.max_right(f, l) != want_mr(l)) fail("seq max_right(f,l)");
      if(t.min_left(f, r) != want_ml(r)) fail("seq min_left(f,r)");
      if(!n) continue;
      if(o.max_right(f, OrdN::ID(l)) != want_mr(l)) fail("ord max_right(f,ID)");
      if(o.min_left(f, OrdN::ID(r)) != want_ml(r)) fail("ord min_left(f,ID)");
      // key=2l は index l の要素 → lower_bound(2l)==l. l==n なら該当なしで lower_bound==n.
      if(o.max_right(f, 2 * l) != want_mr(l)) fail("ord max_right(f,Key)");
      if(o.min_left(f, 2 * r) != want_ml(r)) fail("ord min_left(f,Key)");
    }
    // 引数省略(既定 l=0 / r=size).
    if(t.max_right(f) != want_mr(0)) fail("seq max_right default");
    if(t.min_left(f) != want_ml(n)) fail("seq min_left default");
    if(n && o.max_right(f) != want_mr(0)) fail("ord max_right default");
    if(n && o.min_left(f) != want_ml(n)) fail("ord min_left default");
    t.clear();
    o.clear();
  }
}

// ordered の vector ctor: 未ソート入力を stable_sort してから build する.
void ord_ctor_test(mt19937& rng) {
  for(int iter = 0; iter < 800 && Fc <= 10; ++iter) {
    int n = rng() % 30;
    vector<pair<int, Af>> v;
    set<int> used;
    while((int)v.size() < n) {
      int k = rng() % 1000;
      if(used.insert(k).second) v.emplace_back(k, rnd_af(rng));
    }
    shuffle(v.begin(), v.end(), rng); // 未ソートで渡す(ctor 内の stable_sort を検証).
    Ord t(v);
    map<int, Af> mp(v.begin(), v.end());
    vector<pair<int, Af>> wv(mp.begin(), mp.end()), gv;
    for(auto [k, val]: t.get_vec_pair()) gv.emplace_back(k, val);
    if(gv != wv || t.size() != (int)mp.size()) fail("ordered vector ctor");
    Af want = af_id();
    for(auto& [k, val]: mp) want = af_comp(want, val);
    if(t.all_prod() != want) fail("ordered vector ctor all_prod");
    if(n) {
      int i = rng() % n;
      if(t.key_at(i) != wv[i].first || t.get(Ord::ID(i)) != wv[i].second) fail("ordered vector ctor key_at/get");
    }
    t.clear();
  }
}

// 極値 key(INT_MIN/INT_MAX). 大小比較を差分で行う実装なら符号付きオーバーフローで破綻する.
void extreme_key_test(mt19937& rng) {
  vector<int> ks = {INT_MIN, INT_MIN + 1, -1, 0, 1, INT_MAX - 1, INT_MAX};
  for(int iter = 0; iter < 400 && Fc <= 10; ++iter) {
    Ord t;
    map<int, Af> mp;
    for(int i = 0, m = 1 + rng() % 20; i < m; ++i) {
      int k = ks[rng() % ks.size()];
      if(rng() % 4 == 0) {
        t.erase(k), mp.erase(k);
        continue;
      }
      Af v = rnd_af(rng);
      t.upsert(k, v), mp[k] = v;
    }
    for(int k: ks) {
      if(t.contains(k) != (bool)mp.count(k)) fail("extreme contains");
      if(t.lower_bound(k) != (int)distance(mp.begin(), mp.lower_bound(k))) fail("extreme lower_bound");
      if(t.upper_bound(k) != (int)distance(mp.begin(), mp.upper_bound(k))) fail("extreme upper_bound");
    }
    Af all = af_id(), half = af_id(); // half = key ∈ [INT_MIN, INT_MAX) の積(半開なので INT_MAX を除く).
    for(auto& [k, v]: mp) {
      all = af_comp(all, v);
      if(k < INT_MAX) half = af_comp(half, v);
    }
    if(t.all_prod() != all) fail("extreme all_prod");
    if(t.prod(INT_MIN, INT_MAX) != half) fail("extreme prod(Key,Key)");
    t.clear();
  }
}

// copy コンストラクタ / copy 代入 が deep copy かつ独立か. move と自己代入も併せて検証.
void copy_test(mt19937& rng) {
  for(int iter = 0; iter < 800 && Fc <= 10; ++iter) {
    Ord a;
    map<int, Af> mp;
    for(int i = 0, m = rng() % 25; i < m; ++i) {
      int k = rng() % 30;
      Af v = rnd_af(rng);
      a.upsert(k, v);
      mp[k] = v;
    }
    Ord b = a; // copy ctor
    Ord c;
    c.upsert(999, {1, 1});
    c = a; // copy assign (既存要素は解放される)
    // 期待値は原本 a ではなく ground truth の mp から作る(a を clear した後も比較できるように).
    Af want_sum = af_id();
    for(auto& [k, v]: mp) want_sum = af_comp(want_sum, v);
    auto same = [&](Ord& x, char const* m) {
      if(x.size() != (int)mp.size() || x.all_prod() != want_sum) fail(m);
      vector<pair<int, Af>> wv(mp.begin(), mp.end()), gv;
      for(auto [k, v]: x.get_vec_pair()) gv.emplace_back(k, v);
      if(gv != wv) fail(m);
    };
    same(b, "copy ctor content");
    same(c, "copy assign content");
    // move: 移動元は空になり, 移動先が中身を引き継ぐ(移動先の既存要素は解放される).
    Ord d = std::move(c); // move ctor
    same(d, "move ctor content");
    if(!c.empty()) fail("move ctor: source not emptied");
    Ord g;
    g.upsert(777, {3, 3}); // mp の key は <30 なので 777 は必ず「移動先の既存要素」.
    g = std::move(d);      // move assign
    if(!d.empty()) fail("move assign: source not emptied");
    if(g.contains(777)) fail("move assign: old content not released");
    same(g, "move assign content");
    Ord& gself = g;
    g = gself; // 自己 copy 代入は no-op(this!=&ot ガード). 参照経由で -Wself-assign を避ける.
    same(g, "self copy-assign");
    c = g; // 以降の独立性チェックのため c を復元.
    g.clear(), d.clear();
    // 独立性: 原本を破棄しても複製は無傷.
    ll bsz = b.size();
    Af bsum = b.all_prod();
    a.clear();
    if(b.size() != bsz || b.all_prod() != bsum) fail("copy ctor not independent");
    same(c, "copy assign not independent");
    // 複製を変更しても他方に影響しない.
    b.upsert(12345, {2, 2});
    if(c.contains(12345)) fail("copies share nodes");
    b.clear();
    c.clear();
  }
}

void meld_test(mt19937& rng) {
  for(int iter = 0; iter < 800 && Fc <= 10; ++iter) {
    Ord a, b;
    map<int, Af> ma, mb;
    auto cmb = [](Af x, Af y) { return Af{max(x[0], y[0]), max(x[1], y[1])}; }; // 可換.
    for(int i = 0, na = rng() % 20; i < na; ++i) {
      int k = rng() % 30;
      Af v = rnd_af(rng);
      a.upsert(k, v);
      ma[k] = v;
    }
    for(int i = 0, nb = rng() % 20; i < nb; ++i) {
      int k = rng() % 30;
      Af v = rnd_af(rng);
      b.upsert(k, v);
      mb[k] = v;
    }
    a.meld(b, cmb); // meld 内で validate() 済み.
    map<int, Af> want = ma;
    for(auto& [k, v]: mb) want[k] = want.count(k) ? cmb(want[k], v) : v;
    if(!b.empty()) fail("meld b not empty");
    vector<pair<int, Af>> wv(want.begin(), want.end()), gv;
    for(auto [k, v]: a.get_vec_pair()) gv.emplace_back(k, v);
    if(gv != wv) fail("meld content");
    // 自己 meld は no-op(this==&ot ガード). 外すと自分自身を消費して壊れる.
    Ord& aself = a;
    a.meld(aself, cmb);
    gv.clear();
    for(auto [k, v]: a.get_vec_pair()) gv.emplace_back(k, v);
    if(gv != wv) fail("self meld not no-op");
    a.clear();
  }
}

// 大きめの木で平衡(高さ)が O(log n) に収まるか.
// 注意: __LION_DEBUG 時は各 upsert が内部 validate() で O(N) になり全体 O(N^2) になるため n を落とす.
//       高さの本検証は release ビルド(内部 validate 無効)で大 n を回す.
void balance_test() {
#ifdef __LION_DEBUG
  int n = 3000;
#else
  int n = 300000;
#endif
  int bound = (int)(2.2 * log2(n)) + 4; // delta=5/2 で高さ <= ~2.06 log2 n.
  auto check = [&](char const* name, int h) {
    cerr << "balance: " << left << setw(17) << name << " n=" << n << " height=" << h << " bound=" << bound << "\n";
    if(h > bound) fail(name);
  };
  { // ランダム key.
    OrdS t;
    mt19937 rng(1);
    for(int i = 0; i < n; ++i) t.upsert((int)(rng() % 1000000000), 1);
    check("upsert-random", t.height());
    t.clear();
  }
  { // 昇順/降順 key: 片側 spine ばかり伸ばす, 回転を最も酷使するパターン.
    OrdS t;
    for(int i = 0; i < n; ++i) t.upsert(i, 1);
    check("upsert-ascending", t.height());
    t.clear();
    for(int i = n; i-- > 0;) t.upsert(i, 1);
    check("upsert-descending", t.height());
    t.clear();
  }
  { // index 挿入の偏りパターン.
    SeqS t;
    for(int i = 0; i < n; ++i) t.insert(t.size(), 1);
    check("insert-tail", t.height());
    t.clear();
    for(int i = 0; i < n; ++i) t.insert(0, 1);
    check("insert-head", t.height());
    t.clear();
    for(int i = 0; i < n; ++i) t.insert(t.size() / 2, 1);
    check("insert-middle", t.height());
    t.clear();
  }
}

int main(int argc, char** argv) {
  unsigned seed = argc > 1 ? (unsigned)stoul(argv[1]) : 0u;
  cerr << "seed=" << seed << "\n";
  mt19937 rng(seed);
  order_test();
  seq_test(rng);
  ord_test(rng);
  ord_ctor_test(rng);
  binsearch_test(rng);
  extreme_key_test(rng);
  copy_test(rng);
  meld_test(rng);
  balance_test();
  cerr << (Fc ? to_string(Fc) + " FAILURES\n" : "ALL OK\n");
  return Fc ? 1 : 0;
}
