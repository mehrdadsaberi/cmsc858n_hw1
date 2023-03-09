#include "parallel.h"
#include "random.h"

// For timing parts of your code.
#include "get_time.h"

// For computing sqrt(n)
#include <math.h>

using namespace parlay;

// Some helpful utilities
namespace {

// returns the log base 2 rounded up (works on ints or longs or unsigned
// versions)
template <class T>
size_t log2_up(T i) {
  assert(i > 0);
  size_t a = 0;
  T b = i - 1;
  while (b > 0) {
    b = b >> 1;
    a++;
  }
  return a;
}

}  // namespace


struct ListNode {
  ListNode* next;
  size_t rank;
  ListNode(ListNode* next) : next(next), rank(std::numeric_limits<size_t>::max()) {}

  ListNode* wyllie_next;
  size_t wyllie_dist;

  bool marked;
  ListNode* sampled_next;
  size_t sampled_dist;

};

// Serial List Ranking. The rank of a node is its distance from the
// tail of the list. The tail is the node with `next` field nullptr.
//
// The work/depth bounds are:
// Work = O(n)
// Depth = O(n)
void SerialListRanking(ListNode* head) {
  size_t ctr = 0;
  ListNode* save = head;
  while (head != nullptr) {
    head = head->next;
    ++ctr;
  }
  head = save;
  --ctr;  // last node is distance 0
  while (head != nullptr) {
    head->rank = ctr;
    head = head->next;
    --ctr;
  }
}

// Wyllie's List Ranking. Based on pointer-jumping.
//
// The work/depth bounds of your implementation should be:
// Work = O(n*\log(n))
// Depth = O(\log^2(n))
void WyllieListRanking(ListNode* L, size_t n) {
  size_t k = log2_up(n);

  auto initialize = [&] () {
    parallel_for(0, n, [&](size_t i) {
      L[i].wyllie_next = L[i].next;
      L[i].wyllie_dist = (size_t)(L[i].next != nullptr);
    });
  };
  initialize();

  for(size_t j = 0; j < k; j++) {
      auto update_lst_next = [&] () {
        parallel_for(0, n, [&](size_t i) {
          if(L[i].wyllie_next != nullptr) {
            L[i].wyllie_dist += (L[i].wyllie_next)->wyllie_dist;
            L[i].wyllie_next = (L[i].wyllie_next)->wyllie_next;
          }
        });
      };
      update_lst_next();
  }
}


// Sampling-Based List Ranking
//
// The work/depth bounds of your implementation should be:
// Work = O(n) whp
// Depth = O(\sqrt(n)* \log(n)) whp
void SamplingBasedListRanking(ListNode* L, size_t n, long num_samples=-1, parlay::random r=parlay::random(0)) {
  // Perhaps use a serial base case for small enough inputs?

  if (num_samples == -1) {
    num_samples = sqrt(n);
  }

  auto sample = [&] () {
    parallel_for(0, n, [&](size_t i) {
      if(L[i].next == nullptr || r.ith_rand(i) % ((size_t)(n / num_samples)) == 0) {
        L[i].marked = true;
      }
      else {
        L[i].marked = false;
      }
    });
  };
  sample();

  auto find_next_sampled = [&] () {
    parallel_for(0, n, [&](size_t i) {
      if(L[i].marked) {
        if(L[i].next == nullptr) {
          L[i].sampled_next = nullptr;
          L[i].sampled_dist = 0;
        }
        else {
          size_t cnt = 1;
          ListNode* cur_node = L[i].next;
          while (!cur_node->marked) {
            cur_node = cur_node->next;
            cnt++;
          }
          L[i].sampled_next = cur_node;
          L[i].sampled_dist = cnt;
          cnt--;
          cur_node = L[i].next;
          while (!cur_node->marked) {
            cur_node->sampled_next = L[i].sampled_next;
            cur_node->sampled_dist = cnt;
            cur_node = cur_node->next;
            cnt--;
          }
        }
      }
    });
  };
  find_next_sampled();

  auto calculate_sampled_ranks = [&] () {
    parallel_for(0, n, [&](size_t i) {
      if(L[i].sampled_dist == 0 && L[i].next != nullptr) { 
        //  calculating sampled_dist and sampled_next for nodes
        //    that are before the first sampled node
        size_t cnt = 1;
        ListNode* cur_node = L[i].next;
        while (!cur_node->marked) {
          cur_node = cur_node->next;
          cnt++;
        }
        L[i].sampled_next = cur_node;
        L[i].sampled_dist = cnt;
      }
      if(L[i].marked) {
        //  calculating rank for sampled nodes
        if(L[i].next == nullptr) {
          L[i].rank = 0;
        }
        else {
          size_t cnt = L[i].sampled_dist;
          ListNode* cur_node = L[i].sampled_next;
          while (cur_node != nullptr) {
            cnt += cur_node->sampled_dist;
            cur_node = cur_node->sampled_next;
          }
          L[i].rank = cnt;
        }
      }
    });
  };
  calculate_sampled_ranks();

  auto calculate_unsampled_ranks = [&] () {
    parallel_for(0, n, [&](size_t i) {
      if(!L[i].marked) {
        L[i].rank = L[i].sampled_next->rank + L[i].sampled_dist;
      }
    });
  };
  calculate_unsampled_ranks();

}

