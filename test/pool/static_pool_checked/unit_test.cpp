// static_pool 誤用検知版(static_pool_checked.hpp)のユニットテスト.
// compile: g++ -std=c++23 -O2 -DLION_DEBUG -I<repo>/cpp_library unit_test.cpp
//   LION_DEBUG を付けると "lion/static_pool.hpp" が検知版に差し替わる. 本テストは検知版が対象.
//   assert で落ちることを確かめるので -DNDEBUG は付けないこと.
// run: ./unit_test
//
// 誤用の検知は「assert で abort する」ことなので, 誤用ケースは fork した子プロセスで実行し,
// 子が SIGABRT で死んだかを親が waitpid で見る(death test). fork なら子がプールを壊しても
// 親の状態には影響しない(コピーオンライトの自分のコピーを壊すだけ).
#include "lion/static_pool.hpp"
#include <bits/stdc++.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace std;

#ifndef LION_DEBUG
  #error "this test targets the checked pool: build with -DLION_DEBUG"
#endif

int Fc = 0;
auto fail = [](char const* m) { cerr << "FAIL: " << m << "\n"; ++Fc; };
auto check = [](bool ok, char const* m) { if(!ok) fail(m); };

// 誤用ケース f が assert で abort することを確認する.
void expect_abort(char const* name, auto const& f) {
  pid_t pid = fork();
  if(pid < 0) {
    fail("fork failed");
    return;
  }
  if(pid == 0) { // 子: 誤用を実行する. assert のメッセージは捨てる.
    (void)!freopen("/dev/null", "w", stderr);
    f();
    _exit(0); // ここに来た = 検知漏れ(abort しなかった).
  }
  int st = 0;
  waitpid(pid, &st, 0);
  if(!(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT)) fail(name);
}

// 破棄回数を数える非 trivial な型(free/clear の破棄まわりの検証用).
int dtor_cnt = 0;
struct NT {
  int v;
  constexpr NT(int v): v(v) {}
  ~NT() { ++dtor_cnt; }
};

// プールは巨大になりうるのでグローバルに置く(テストでは N を小さくしてある).
static_pool<int, 64> pi;
static_pool<NT, 16> pnt;
static_pool<int, 4> pcap;

using Id = static_pool<int, 64>::id_type;

// fast 版同様 trivially destructible であること(グローバルプールが終了時に破棄されない性質の維持).
static_assert(is_trivially_destructible_v<static_pool<int, 64>>);
static_assert(is_trivially_destructible_v<static_pool<NT, 16>>); // 非 trivial T でもプール自体は trivial.

int main() {
  // ===== 正常系 =====
  {
    Id a = pi.alloc(10), b = pi.alloc(20), c = pi.alloc(30);
    check(pi.size() == 3, "size after 3 allocs");
    check(pi[a] == 10 && pi[b] == 20 && pi[c] == 30, "values after alloc");
    pi[b] = 99;
    check(pi[b] == 99, "write through operator[]");
    check(*pi.ptr(a) == 10, "ptr()");
    check(!(a == b) && !(a == pi.NIL), "handles are distinct / not NIL");
    pi.free(b);
    check(pi.size() == 2, "size after free");
    check(pi[a] == 10 && pi[c] == 30, "other handles stay valid after a free");
    pi.free(a), pi.free(c);
    check(pi.size() == 0, "size after freeing all");
  }
  // 再確保: スロットが使い回されても新しいハンドルは正しく動く(古い方の検知は死亡テスト側).
  {
    pi.clear();
    Id a = pi.alloc(1);
    pi.free(a);
    Id b = pi.alloc(2);
    check(pi[b] == 2, "reallocated slot works");
    check(!(a == b), "handle differs after realloc (generation bumped)");
    pi.free(b);
  }
  // fast 版(id_type=uint32_t)で書けるコードが検知版でもそのまま通ること
  // (set/map のキー・ソート・unordered 系). 実装差し替えで利用側が壊れないことの担保.
  {
    pi.clear();
    vector<Id> v;
    for(int i = 0; i < 5; ++i) v.push_back(pi.alloc(i));
    set<Id> s(v.begin(), v.end());
    map<Id, int> m;
    for(Id h: v) m[h] = pi[h];
    unordered_set<Id> us(v.begin(), v.end());
    sort(v.begin(), v.end());
    check(s.size() == 5 && m.size() == 5 && us.size() == 5, "handle usable in set/map/unordered/sort");
    check(is_sorted(v.begin(), v.end()), "handles are sortable");
    for(Id h: v) pi.free(h);
  }
  // clear: 全 free 後に先頭から再利用できる.
  {
    pi.clear();
    for(int i = 0; i < 10; ++i) pi.alloc(i);
    check(pi.size() == 10, "size before clear");
    pi.clear();
    check(pi.size() == 0, "size after clear");
    Id a = pi.alloc(7);
    check(pi[a] == 7, "alloc works after clear");
    pi.free(a);
  }
  // 非 trivial T: free で破棄される. fast=true なら破棄を捨てる(fast 版と同じ挙動).
  {
    pnt.clear();
    dtor_cnt = 0;
    auto x = pnt.alloc(5);
    check(pnt[x].v == 5, "non-trivial value");
    pnt.free(x);
    check(dtor_cnt == 1, "free destroys non-trivial T");

    static_pool<NT, 16>::fast = true;
    dtor_cnt = 0;
    auto y = pnt.alloc(6);
    pnt.free(y);
    check(dtor_cnt == 0, "fast=true skips destructor");
    static_pool<NT, 16>::fast = false;
    pnt.clear();
  }

  // ===== 誤用検知(death test) =====
  // 本命: free したスロットが再確保された後に古いハンドルで触る.
  // fast 版だと a と b が同じ index になり素通りしてしまうケース.
  expect_abort("stale handle after realloc not detected", [] {
    static_pool<int, 64> p;
    Id a = p.alloc(1);
    p.free(a);
    Id b = p.alloc(2);
    (void)b;
    int volatile v = p[a]; // ← ここで abort してほしい.
    (void)v;
  });
  expect_abort("use after free not detected", [] {
    static_pool<int, 64> p;
    Id a = p.alloc(1);
    p.free(a);
    int volatile v = p[a];
    (void)v;
  });
  expect_abort("double free not detected", [] {
    static_pool<int, 64> p;
    Id a = p.alloc(1);
    p.free(a);
    p.free(a);
  });
  expect_abort("free of stale handle after realloc not detected", [] {
    static_pool<int, 64> p;
    Id a = p.alloc(1);
    p.free(a);
    Id b = p.alloc(2);
    (void)b;
    p.free(a);
  });
  expect_abort("NIL access not detected", [] {
    static_pool<int, 64> p;
    int volatile v = p[static_pool<int, 64>::NIL];
    (void)v;
  });
  expect_abort("NIL free not detected", [] {
    static_pool<int, 64> p;
    p.free(static_pool<int, 64>::NIL);
  });
  expect_abort("never-allocated handle not detected", [] {
    static_pool<int, 64> p;
    p.alloc(1);
    int volatile v = p[Id{5, 0}]; // 切り出していない index.
    (void)v;
  });
  expect_abort("handle used after clear not detected", [] {
    static_pool<int, 64> p;
    Id a = p.alloc(1);
    p.clear();
    int volatile v = p[a];
    (void)v;
  });
  // free 済み領域への stale ポインタ経由の書き込みを, 毒値の破壊として次の alloc で検知する.
  expect_abort("write to freed slot not detected", [] {
    static_pool<int, 64> p;
    Id a = p.alloc(1);
    int* q = p.ptr(a);
    p.free(a);
    *q = 12345; // 解放済み領域への書き込み(ダングリングポインタ).
    p.alloc(2); // ← 毒値が壊れているのでここで abort してほしい.
  });
  expect_abort("capacity overflow not detected", [] {
    static_pool<int, 4> p;
    for(int i = 0; i < 5; ++i) p.alloc(i); // N=4 を超える.
  });

  if(Fc == 0) cout << "ALL OK\n";
  else cout << Fc << " FAILED\n";
  return Fc != 0;
}
