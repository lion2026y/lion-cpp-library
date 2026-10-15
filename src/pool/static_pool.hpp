#pragma once
// static_pool のディスパッチャ(使い方は document/static_pool.md).
// 利用側はこのファイル(= "lion/static_pool.hpp")だけを include する.
//   通常ビルド : static_pool_fast.hpp    (検知なしの本番用)
//   LION_DEBUG : static_pool_checked.hpp (誤用検知版)
// 両者は同じ API(alloc/free/clear/operator[]/ptr/size/fast/NIL/id_type)を提供すること.
#ifdef LION_DEBUG
  #include "src/pool/static_pool_checked.hpp"
#else
  #include "src/pool/static_pool_fast.hpp"
#endif
