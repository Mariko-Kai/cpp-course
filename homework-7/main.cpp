#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

struct Config {
  int threads;
  int top_m;
  int min_len;
  fs::path dir_path;
};

bool parse_args(int argc, char *argv[], Config &config) {
  if (argc < 2)
    return false;

  config.threads = 1;
  config.top_m = 10;
  config.min_len = 1;
  config.dir_path = argv[argc - 1];

  for (int i = 1; i < argc - 1; ++i) {
    std::string arg = argv[i];
    if (arg == "--threads" && i + 1 < argc - 1) {
      config.threads = std::stoi(argv[++i]);
    } else if (arg == "--top" && i + 1 < argc - 1) {
      config.top_m = std::stoi(argv[++i]);
    } else if (arg == "--minlen" && i + 1 < argc - 1) {
      config.min_len = std::stoi(argv[++i]);
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return false;
    }
  }

  if (config.threads < 1 || config.top_m < 1 || config.min_len < 1) {
    std::cerr << "Parameters must be >= 1\n";
    return false;
  }

  if (!fs::exists(config.dir_path) || !fs::is_directory(config.dir_path)) {
    std::cerr << "Directory not found or is not a directory: "
              << config.dir_path << "\n";
    return false;
  }

  return true;
}

class ThreadSafeQueue {
private:
  std::queue<fs::path> q;
  std::mutex mtx;
  std::condition_variable cv;
  bool producer_done = false;

public:
  void push(fs::path file_path) {
    std::lock_guard<std::mutex> lock(mtx);
    q.push(std::move(file_path));
    cv.notify_one();
  }

  bool pop(fs::path &file_path) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return !q.empty() || producer_done; });

    if (q.empty() && producer_done) {
      return false;
    }

    file_path = std::move(q.front());
    q.pop();
    return true;
  }

  void set_done() {
    std::lock_guard<std::mutex> lock(mtx);
    producer_done = true;
    cv.notify_all();
  }
};

bool is_word_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

void parse_line_and_count(
    const std::string &line, int min_len,
    std::unordered_map<std::string, uint64_t> &local_map) {
  std::string current_word;
  for (char c : line) {
    if (is_word_char(c)) {
      current_word +=
          static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else {
      if (current_word.length() >= static_cast<size_t>(min_len)) {
        local_map[current_word]++;
      }
      current_word.clear();
    }
  }

  if (current_word.length() >= static_cast<size_t>(min_len)) {
    local_map[current_word]++;
  }
}

const int NUM_SHARDS = 16;
std::vector<std::unordered_map<std::string, uint64_t>>
    global_counts(NUM_SHARDS);
std::vector<std::mutex> global_counts_mtx(NUM_SHARDS);

size_t get_shard_index(const std::string &word) {
  return std::hash<std::string>{}(word) % NUM_SHARDS;
}

void consumer(ThreadSafeQueue &queue, int min_len) {
  std::unordered_map<std::string, uint64_t> local_counts;
  fs::path file_path;

  while (queue.pop(file_path)) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
      std::cerr << "Failed to open file: " << file_path << "\n";
      continue;
    }

    std::string line;
    while (std::getline(file, line)) {
      parse_line_and_count(line, min_len, local_counts);
    }
  }

  for (const auto &[word, count] : local_counts) {
    size_t shard = get_shard_index(word);
    std::lock_guard<std::mutex> lock(global_counts_mtx[shard]);
    global_counts[shard][word] += count;
  }
}

int main(int argc, char *argv[]) {
  Config config;
  if (!parse_args(argc, argv, config)) {
    std::cerr << "Usage: " << argv[0]
              << " --threads K --top M --minlen L <path>\n";
    return 1;
  }

  ThreadSafeQueue queue;
  std::vector<std::thread> workers;

  for (int i = 0; i < config.threads; ++i) {
    workers.emplace_back(consumer, std::ref(queue), config.min_len);
  }

  try {
    for (const auto &entry :
         fs::recursive_directory_iterator(config.dir_path)) {
      if (fs::is_regular_file(entry)) {
        queue.push(entry.path());
      }
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << "\n";
  }

  queue.set_done();

  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  std::vector<std::pair<std::string, uint64_t>> sorted_words;
  for (int i = 0; i < NUM_SHARDS; ++i) {
    for (const auto &pair : global_counts[i]) {
      sorted_words.push_back(pair);
    }
  }

  size_t count_to_show =
      std::min(static_cast<size_t>(config.top_m), sorted_words.size());

  std::partial_sort(sorted_words.begin(), sorted_words.begin() + count_to_show,
                    sorted_words.end(),
                    [](const std::pair<std::string, uint64_t> &a,
                       const std::pair<std::string, uint64_t> &b) {
                      if (a.second != b.second) {
                        return a.second > b.second;
                      }
                      return a.first < b.first;
                    });

  for (size_t i = 0; i < count_to_show; ++i) {
    std::cout << sorted_words[i].first << " " << sorted_words[i].second << "\n";
  }

  return 0;
}
