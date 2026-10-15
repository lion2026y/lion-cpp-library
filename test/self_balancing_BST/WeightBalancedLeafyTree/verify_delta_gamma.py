# WeightBalancedLeafyTree の平衡パラメータ (delta, gamma) の正当性を z3 で検証する.
#
# 検証内容: merge_sub の single/double 回転が「回転後に木が平衡に戻る」ことを, 回転に入る
# 前提条件のもとで全整数重みについて確認する(反例探索が UNSAT なら正当).
#
# モデル(merge_sub の "l too heavy" 枝, "r too heavy" は左右対称):
#   node l=(a,b) は元々平衡. c = merge(b, r) は平衡で重み wc=b+r. 回転は !like(wc,a) のとき.
#   c の子 (c0,c1) は「重み wc の任意の平衡木」として過大近似する(実際の join 結果はこの部分集合
#   なので, これで反例が無ければ実アルゴリズムでも平衡は保たれる=健全な十分条件).
#
# 実行: uv run --with z3-solver python verify_delta_gamma.py
#   (または pip install z3-solver 後 python verify_delta_gamma.py)
from __future__ import annotations

from z3 import And, ArithRef, BoolRef, Ints, Not, Solver, unsat

# 検証したいパラメータ: delta = Dn/Dd, gamma = Gn/Gd.
Dn: int = 5
Dd: int = 2
Gn: int = 3
Gd: int = 2


def like(x: ArithRef, y: ArithRef) -> BoolRef:
    return Dd * x <= Dn * y  # x <= delta*y (平衡側).


def bal(x: ArithRef, y: ArithRef) -> BoolRef:
    return And(like(x, y), like(y, x))


def single(x: ArithRef, y: ArithRef) -> BoolRef:
    return Gd * x <= Gn * y  # x <= gamma*y (single rotation で足りる).


def check(name: str, cons: list[BoolRef], goal: BoolRef) -> bool:
    s: Solver = Solver()
    s.add(cons)
    s.add(Not(goal))  # 反例 = 回転後に平衡でない.
    if s.check() == unsat:
        print(f"[OK]   {name}")
        return True
    print(f"[FAIL] {name}: counterexample {s.model()}")
    return False


ok: bool = True

# ===== l too heavy: l=(a,b) 平衡, c=merge(b,r), 子 (c0,c1) =====
a, b, r, c0, c1, c0l, c0r = Ints("a b r c0 c1 c0l c0r")
pos: list[BoolRef] = [a >= 1, b >= 1, r >= 1, c0 >= 1, c1 >= 1]
entry: list[BoolRef] = [
    bal(a, b),            # l は元々平衡.
    Not(like(a + b, r)),  # この枝の条件: l が r より delta 倍超重い.
    c0 + c1 == b + r,     # join は総重み保存: wc = b+r.
    bal(c0, c1),          # join 結果 c は平衡.
    Not(like(b + r, a)),  # 回転が必要: c が a より重すぎ.
]
# single rotation: new_l=(a,c0), root=((a,c0),c1).
ok &= check("l-heavy single", pos + entry + [single(c0, c1)],
            And(bal(a, c0), bal(a + c0, c1)))
# double rotation: c0=(c0l,c0r), root=((a,c0l),(c0r,c1)).
ok &= check("l-heavy double",
            pos + entry + [Not(single(c0, c1)), c0l >= 1, c0r >= 1, c0l + c0r == c0, bal(c0l, c0r)],
            And(bal(a, c0l), bal(c0r, c1), bal(a + c0l, c0r + c1)))

# ===== r too heavy(左右対称): r=(a2,b2) 平衡, c=merge(l,a2), 子 (d0,d1) =====
a2, b2, l2, d0, d1, d1l, d1r = Ints("a2 b2 l2 d0 d1 d1l d1r")
pos2: list[BoolRef] = [a2 >= 1, b2 >= 1, l2 >= 1, d0 >= 1, d1 >= 1]
entry2: list[BoolRef] = [
    bal(a2, b2),
    Not(like(a2 + b2, l2)),
    d0 + d1 == l2 + a2,
    bal(d0, d1),
    Not(like(l2 + a2, b2)),
]
# single: new_r=(d1,b2), root=(d0,(d1,b2)).
ok &= check("r-heavy single", pos2 + entry2 + [single(d1, d0)],
            And(bal(d1, b2), bal(d0, d1 + b2)))
# double: d1=(d1l,d1r), root=((d0,d1l),(d1r,b2)).
ok &= check("r-heavy double",
            pos2 + entry2 + [Not(single(d1, d0)), d1l >= 1, d1r >= 1, d1l + d1r == d1, bal(d1l, d1r)],
            And(bal(d0, d1l), bal(d1r, b2), bal(d0 + d1l, d1r + b2)))

print()
print(f"delta = {Dn}/{Dd},  gamma = {Gn}/{Gd}")
print("VERDICT:", "VALID — every rotation restores balance." if ok
      else "INVALID — see counterexamples above.")
