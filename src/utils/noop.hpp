#pragma once

// 任意の引数を受け取って何もしない関数オブジェクト.
inline constexpr auto noop = [](auto&&...) {};
