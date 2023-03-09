#include "parallel.h"

using namespace parlay;

// A serial implementation for checking correctness.
//
// Work = O(n)
// Depth = O(n)
template <class T, class F>
T scan_inplace_serial(T *A, size_t n, const F& f, T id) {
  T cur = id;
  for (size_t i=0; i<n; ++i) {
    T next = f(cur, A[i]);
    A[i] = cur;
    cur = next;
  }
  return cur;
}

template <class T>
T serial_reduce(T *A, size_t n) {
  T tot = 0;
  for (size_t i=0; i<n; ++i) {
    tot += A[i];
  }
  return tot;
}

template <class T, class F>
T scan_up(T *A, size_t n, T *L, const F& f) {
  if(n <= 10000 * 1000) {
    return serial_reduce(A, n);
  }
  if(n == 1) {
    return A[0];
  }
  T v1, v2;
  auto f1 = [&]() { v1 = scan_up(A, n / 2, L, f); };
  auto f2 = [&]() { v2 = scan_up(A + n / 2, n - n / 2, L + n / 2, f); };
  par_do(f1, f2);
  L[n / 2 - 1] = v1;

  return f(v1, v2);
}

template <class T, class F>
void scan_down(T *A, size_t n, T *L, const F& f, T s) {
  if(n <= 10000 * 1000) {
    scan_inplace_serial(A, n, f, s);
    return ;
  }
  if(n == 1) {
    A[0] = s;
    return ;
  }
  auto f1 = [&]() { scan_down(A, n / 2, L, f, s); };
  auto f2 = [&]() { scan_down(A + n / 2, n - n / 2, L + n / 2, f, f(s, L[n / 2 - 1])); };
  par_do(f1, f2);
  return ;
}


// Parallel in-place prefix sums. Your implementation can allocate and
// use an extra n*sizeof(T) bytes of memory.
//
// The work/depth bounds of your implementation should be:
// Work = O(n)
// Depth = O(\log(n))
template <class T, class F>
T scan_inplace(T *A, size_t n, const F& f, T id) {
  T* L = (T*)malloc(n * sizeof(T));
  T total = scan_up(A, n, L, f);
  scan_down(A, n, L, f, id);
  return total;
}
