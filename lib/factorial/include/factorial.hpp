#pragma once

// TODO: delete this after some other tests are set up
// NOLINTNEXTLINE(misc-no-recursion)
inline int factorial(int number) { return number <= 1 ? 1 : factorial(number - 1) * number; }