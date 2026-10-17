#pragma once
#include "lion/noop.hpp"
#include <bits/stdc++.h>
using namespace std;

// 区間 (と値) を Set で管理する構造体.
// valueが同じな区間は自動的につながる.
// add: 区間が追加されたときの影響を反映する, del: 区間が削除されたときの影響を反映する.
// (重要) add と del は互いに逆関数でなければならない.
// clang-format off
template<class T> constexpr T default_e() { return T{}; }
// clang-format on
template<class T, class Value = int,
         auto add = noop, auto del = noop,
         auto f_e = default_e<Value>>
struct interval_set {
  static_assert(is_invocable_r_v<void, decltype(add), T, T, Value>, "add must work as void(T,T,Value)");
  static_assert(is_invocable_r_v<void, decltype(del), T, T, Value>, "del must work as void(T,T,Value)");
  static_assert(is_invocable_r_v<Value, decltype(f_e)>, "f_e must work as Value()");

  struct node;
  // 区間どうしは交わらないので左端だけで順序付ける. is_transparent により T のまま探索できる.
  struct comp {
    using is_transparent = void;
    constexpr bool operator()(node const& a, node const& b) const { return a.l < b.l; }
    constexpr bool operator()(node const& a, T const& p) const { return a.l < p; }
    constexpr bool operator()(T const& p, node const& a) const { return p < a.l; }
  };
  using Itr = set<node, comp>::iterator;

  static inline Value const e = f_e();
  set<node, comp> st;

  constexpr interval_set() {}
  constexpr interval_set(vector<Value> const& v) {
    vector<node> vec;
    int r = 1;
    for(int l = 0; l < (int)v.size();) {
      while(r < (int)v.size() && v[r] == v[l]) r++;
      auto& nd = vec.emplace_back(l, r, v[l]);
      add(nd.l, nd.r, nd.v);
      l = r;
    }
    st = set<node, comp>(vec.begin(), vec.end());
  }
  // Tが共通なinterval_set二つとそのValueの二項演算(とv=eの時に追加しないか)をとるコンストラクタ.
  template<class V1, auto add1, auto del1, auto f_e1, class V2, auto add2, auto del2, auto f_e2>
  constexpr interval_set(interval_set<T, V1, add1, del1, f_e1> const& r1,
                         interval_set<T, V2, add2, del2, f_e2> const& r2,
                         auto&& op, bool unpush_e = false) {
    static_assert(is_invocable_r_v<Value, decltype(op), V1, V2>, "op must work as Value(V1,V2)");
    auto it1 = r1.begin();
    auto it2 = r2.begin();
    if(!r1.not_end(it1) && !r2.not_end(it2)) return;
    vector<node> vec;
    T l;
    // 次の区間の左端に進める. 少なくとも一方が終端でないときに呼ぶ.
    auto cal_l = [&] { l = (!r2.not_end(it2) || (r1.not_end(it1) && it1->l < it2->l)) ? it1->l : it2->l; };
    cal_l();
    while(r1.not_end(it1) || r2.not_end(it2)) {
      bool b1 = r1.not_end(it1) && it1->l <= l, b2 = r2.not_end(it2) && it2->l <= l;
      if(!b1 && !b2) {
        cal_l();
        continue;
      }
      // 次の境界: l を含む区間の右端と, まだ始まっていない区間の左端のうち最小のもの.
      T r = b1 ? it1->r : it2->r;
      if(b2) r = min(r, it2->r);
      if(!b1 && r1.not_end(it1)) r = min(r, it1->l);
      if(!b2 && r2.not_end(it2)) r = min(r, it2->l);
      Value v = invoke(op, b1 ? it1->v : r1.e, b2 ? it2->v : r2.e);
      if(v != e || !unpush_e) {
        if(!vec.empty() && vec.back().v == v && vec.back().r == l) vec.back().r = r;
        else vec.emplace_back(l, r, move(v));
      }
      if(b1 && it1->r == r) ++it1;
      if(b2 && it2->r == r) ++it2;
      l = r;
    }
    // 隣接する同じ値の区間をつなげ終えてから, ノードごとに add を呼ぶ.
    for(auto& nd: vec) add(nd.l, nd.r, nd.v);
    st = set<node, comp>(vec.begin(), vec.end());
  }
  // コピー: コピーした区間ごとに add を呼ぶ.
  constexpr interval_set(interval_set const& o): st(o.st) {
    for(auto& [l, r, v]: st) add(l, r, v);
  }
  // ムーブ: 区間を移すだけなので add / del は呼ばない. 移動元は空になる.
  constexpr interval_set(interval_set&& o) noexcept: st(move(o.st)) { o.st.clear(); }
  // 代入 (コピー / ムーブ共通): 新しい区間は o を作るときに処理済み.
  // 元の区間は o と入れ替わって o とともに破棄されるので, そこで del が呼ばれる.
  constexpr interval_set& operator=(interval_set o) noexcept {
    st.swap(o.st);
    return *this;
  }
  // 破棄: 残っている区間ごとに del を呼ぶ.
  constexpr ~interval_set() {
    for(auto& [l, r, v]: st) del(l, r, v);
  }

  constexpr size_t size() const { return st.size(); }

  // 先頭・末尾のイテレータを返す.
  constexpr Itr begin() const { return st.begin(); }
  constexpr Itr end() const { return st.end(); }

  constexpr bool not_end(Itr it) const { return it != end(); }

  // p を含む区間のイテレータを返す.
  // 存在しなければ st.end() を返す.
  constexpr Itr get(T const& p) const {
    auto it = st.upper_bound(p);
    if(it == st.begin()) return st.end();
    if(p < (--it)->r) return it;
    else return st.end();
  }

  // p 以上の点を含む区間のうち, 最も左にある区間のイテレータを返す.
  constexpr Itr lower_bound(T const& p) const {
    auto it = st.upper_bound(p);
    if(it == st.begin()) return it;
    auto pit = prev(it);
    if(p < pit->r) return pit;
    else return it;
  }

  // p を含む区間が存在するか / [l, r) 全体を含む区間が存在するか.
  // l == r のときは true を返す.
  constexpr bool covered(T const& p) const { return not_end(get(p)); }
  constexpr bool covered(T const& l, T const& r) const {
    assert(l <= r);
    if(l == r) return true;
    auto it = get(l);
    return not_end(it) && r <= it->r;
  }

  // p と q が同じ区間に含まれるか.
  constexpr bool same(T const& p, T const& q) const {
    auto getp = get(p), getq = get(q);
    return getp == getq && not_end(getp);
  }

  // p を含む区間の値を返す.
  // 存在しなければ e を返す.
  constexpr Value const& get_val(T const& p) const {
    auto it = get(p);
    if(not_end(it)) return it->v;
    else return e;
  }
  constexpr Value const& operator[](T const& p) const { return get_val(p); }

  // p 以上で, どの区間にも含まれない最小の点 (mex) を返す.
  // 値の異なる区間が隣接しているときはそれらをたどるので, たどった区間数を k として O(log n + k).
  constexpr T get_mex(T const& p = 0) const {
    auto it = get(p);
    if(!not_end(it)) return p;
    T x = it->r;
    while(not_end(++it) && it->l == x) x = it->r;
    return x;
  }

  // [l, r) を値 v で更新する (v を省略すると値 e で [l, r) を挿入する).
  // v は値で受け取る (集合内の値への参照を渡されても, 途中でそのノードが消えて壊れないように).
  constexpr void update(T l, T r, Value v = e) {
    if(l >= r) return;
    auto it = st.lower_bound(l);
    // 左隣の区間は, [l, r) と重なるか, l で接していて値が同じときだけ処理する.
    if(it != st.begin()) {
      auto pit = prev(it);
      if(l < pit->r || (pit->r == l && pit->v == v)) it = pit;
    }
    while(not_end(it) && it->l <= r) {
      if(it->v == v) {
        l = min(l, it->l), r = max(r, it->r);
        erase_sub(it++);
      } else {
        if(it->l == r) break;
        it = cut(it, l, r);
      }
    }
    emplace_sub(it, move(l), move(r), move(v));
  }
  // [l, r) を Value(args...) で更新する. explicit なコンストラクタや複数の引数からも構築できる.
  template<class... Args>
    requires constructible_from<Value, Args...>
  constexpr void update(T l, T r, Args&&... args) { update(l, r, Value(forward<Args>(args)...)); }

  // [l, r) を削除する.
  // l, r は値で受け取る (it->l などノード内の値を渡されても, 途中でそのノードが消えて壊れないように).
  constexpr void erase(T l, T r) {
    if(l >= r) return;
    auto it = st.lower_bound(l);
    if(it != st.begin() && prev(it)->r > l) --it;
    while(not_end(it) && it->l < r) it = cut(it, l, r);
  }

private:
  // 区間 [l, r) を値 v で挿入する (it は挿入位置のヒント)
  // 引数は値で受け取る (呼び出し側で型を間違えたときのエラーを分かりやすくするため)
  constexpr void emplace_sub(Itr it, T l, T r, Value v) {
    assert(l < r);
    add(l, r, v);
    st.emplace_hint(it, move(l), move(r), move(v));
  }
  constexpr void erase_sub(Itr it) {
    del(it->l, it->r, it->v);
    st.erase(it);
  }
  // it の区間から [l, r) と重なる部分を取り除く. 残った部分にはノードを再利用する.
  // 返り値: 右側が残ればそのイテレータ, 残らなければ次の区間のイテレータ.
  constexpr Itr cut(Itr it, T const& l, T const& r) {
    del(it->l, it->r, it->v);
    auto nh = st.extract(it++);
    node& nd = nh.value();
    if(r < nd.r) {
      if(nd.l < l) emplace_sub(it, nd.l, l, nd.v); // 両側が残るときだけ左側を新しく確保する.
      nd.l = r;
      add(nd.l, nd.r, nd.v);
      return st.insert(it, move(nh));
    }
    if(nd.l < l) {
      nd.r = l;
      add(nd.l, nd.r, nd.v);
      st.insert(it, move(nh));
    }
    return it;
  }

  friend ostream& operator<<(ostream& s, interval_set const& ins) {
    for(auto& nd: ins.st) s << nd;
    return s;
  }

public:
  struct node {
    T l, r;
    Value v;
    friend ostream& operator<<(ostream& s, node const& nd) {
      return s << "([" << nd.l << ", " << nd.r << "): " << nd.v << ")";
    }
  };
};
