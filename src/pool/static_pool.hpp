#pragma once
// static_pool のディスパッチャ. 利用側はこのファイル(= "lion/static_pool.hpp")だけを include する.
//   通常ビルド    : static_pool_fast.hpp    (intrusive free list, 検知なしの本番用)
//   LION_DEBUG : static_pool_checked.hpp (世代付きハンドルで free 後使用/二重 free 等を assert 検知)
// どちらも同じ API(alloc/free/clear/operator[]/ptr/size/fast/NIL/id_type)を提供するので,
// 利用側は再コンパイルするだけで検知版に差し替わる.
// ハンドル型 static_pool_id は実装ごとに異なる(fast=uint32_t / checked={idx,gen})ため,
// 利用側は必ず static_pool<T,N>::id_type(または static_pool_id)で受けること.
#ifdef LION_DEBUG
  #include "src/pool/static_pool_checked.hpp"
#else
  #include "src/pool/static_pool_fast.hpp"
#endif
