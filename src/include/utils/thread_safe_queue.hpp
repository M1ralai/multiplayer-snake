#ifndef HCB_MSNAKE_THREAD_SAFE_QUEUE
#define HCB_MSNAKE_THREAD_SAFE_QUEUE

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T> class ThreadSafeQueue {
private:
  std::queue<T> queue;
  mutable std::mutex mtx;
  std::condition_variable cv;

public:
  ThreadSafeQueue() = default;
  ~ThreadSafeQueue() = default;

  // Kopyalamayı engelle (Mutex güvenliği için)
  ThreadSafeQueue(const ThreadSafeQueue &) = delete;
  ThreadSafeQueue &operator=(const ThreadSafeQueue &) = delete;

  // Kuyruğa yeni veri ekler ve bekleyen thread'i uyandırır
  void Push(T value) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      queue.push(std::move(value));
    }
    cv.notify_one();
  }

  // Non-blocking: Kuyrukta eleman varsa alır true döner, yoksa beklemeden false döner
  bool TryPop(T &value) {
    std::lock_guard<std::mutex> lock(mtx);
    if (queue.empty()) {
      return false;
    }
    value = std::move(queue.front());
    queue.pop();
    return true;
  }

  // Blocking: Kuyrukta eleman olana kadar bekler
  T WaitAndPop() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return !queue.empty(); });
    T value = std::move(queue.front());
    queue.pop();
    return value;
  }

  bool Empty() const {
    std::lock_guard<std::mutex> lock(mtx);
    return queue.empty();
  }

  size_t Size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return queue.size();
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mtx);
    while (!queue.empty()) {
      queue.pop();
    }
  }
};

#endif // !HCB_MSNAKE_THREAD_SAFE_QUEUE
