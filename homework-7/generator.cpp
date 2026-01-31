#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

struct Args {
    std::string out_dir = "test_logs";
    int files = 20;
    int mib_per_file = 5;
    int vocab = 2000;
    double skew = 1.2;      // ~ Zipf exponent: 0 = равномерно, 1..2 = сильно скошено
    uint64_t seed = 0;      // 0 => по времени
    int min_word_len = 3;
    int max_word_len = 12;
};

static void print_usage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [options]\n"
        "Options:\n"
        "  --out DIR         output directory (default: test_logs)\n"
        "  --files N         number of files (default: 20)\n"
        "  --mib SIZE        size per file in MiB (default: 5)\n"
        "  --vocab V         vocabulary size (default: 2000)\n"
        "  --skew S          frequency skew (default: 1.2)\n"
        "  --seed X          random seed, 0 = time-based (default: 0)\n"
        "  --minlen L        min generated word length (default: 3)\n"
        "  --maxlen L        max generated word length (default: 12)\n"
        "\nExamples:\n"
        "  " << prog << " --out data --files 100 --mib 20 --vocab 50000 --skew 1.3 --seed 42\n";
}

static bool starts_with(std::string_view s, std::string_view pref) {
    return s.size() >= pref.size() && s.substr(0, pref.size()) == pref;
}

static bool parse_args(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; i++) {
        std::string key = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };

        if (key == "--help" || key == "-h") {
            print_usage(argv[0]);
            return false;
        } else if (key == "--out") {
            a.out_dir = need("--out");
        } else if (key == "--files") {
            a.files = std::stoi(need("--files"));
        } else if (key == "--mib") {
            a.mib_per_file = std::stoi(need("--mib"));
        } else if (key == "--vocab") {
            a.vocab = std::stoi(need("--vocab"));
        } else if (key == "--skew") {
            a.skew = std::stod(need("--skew"));
        } else if (key == "--seed") {
            a.seed = static_cast<uint64_t>(std::stoull(need("--seed")));
        } else if (key == "--minlen") {
            a.min_word_len = std::stoi(need("--minlen"));
        } else if (key == "--maxlen") {
            a.max_word_len = std::stoi(need("--maxlen"));
        } else {
            std::cerr << "Unknown option: " << key << "\n";
            print_usage(argv[0]);
            std::exit(2);
        }
    }
    if (a.files <= 0 || a.mib_per_file <= 0 || a.vocab <= 0) {
        std::cerr << "files/mib/vocab must be > 0\n";
        std::exit(2);
    }
    if (a.min_word_len < 1 || a.max_word_len < a.min_word_len) {
        std::cerr << "Invalid minlen/maxlen\n";
        std::exit(2);
    }
    return true;
}

static std::string rand_word(std::mt19937_64& rng, int minlen, int maxlen) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<int> len_dist(minlen, maxlen);
    std::uniform_int_distribution<int> ch_dist(0, 25);

    int len = len_dist(rng);
    std::string w;
    w.reserve(static_cast<size_t>(len));
    for (int i = 0; i < len; i++) w.push_back(alphabet[ch_dist(rng)]);
    return w;
}

static std::string maybe_mutate(std::mt19937_64& rng, const std::string& base) {
    // Добавляем "логовый" шум: цифры, _, Camel-ish, смесь
    std::uniform_int_distribution<int> p(0, 99);
    int x = p(rng);
    if (x < 70) return base; // чаще без мутации

    std::string w = base;
    if (x < 80) {
        // суффикс _123 или _ab12
        std::uniform_int_distribution<int> d(0, 9999);
        w += "_";
        w += std::to_string(d(rng));
    } else if (x < 90) {
        // вставка цифры внутрь
        if (!w.empty()) {
            std::uniform_int_distribution<size_t> pos(0, w.size() - 1);
            std::uniform_int_distribution<int> dig(0, 9);
            w.insert(w.begin() + static_cast<std::ptrdiff_t>(pos(rng)), char('0' + dig(rng)));
        }
    } else {
        // "слегка" поменять регистр
        if (!w.empty()) w[0] = char(std::toupper(static_cast<unsigned char>(w[0])));
    }
    return w;
}

static std::string rand_ip(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> b(1, 254);
    std::ostringstream oss;
    oss << b(rng) << "." << b(rng) << "." << b(rng) << "." << b(rng);
    return oss.str();
}

static std::string rand_level(std::mt19937_64& rng) {
    static const char* levels[] = {"INFO", "WARN", "ERROR", "DEBUG", "TRACE"};
    // Смещаем вероятности: INFO чаще
    std::discrete_distribution<int> d({50, 15, 12, 18, 5});
    return levels[d(rng)];
}

static std::string rand_punct(std::mt19937_64& rng) {
    static const char* p[] = {" ", " ", " ", " ", " ", " - ", " | ", " : ", " :: ", ", ", "; ", "  "};
    std::uniform_int_distribution<int> d(0, (int)(sizeof(p)/sizeof(p[0]) - 1));
    return p[d(rng)];
}

int main(int argc, char** argv) {
    Args a;
    if (!parse_args(argc, argv, a)) return 0;

    uint64_t seed = a.seed;
    if (seed == 0) {
        seed = static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        );
    }
    std::mt19937_64 rng(seed);

    fs::path out = a.out_dir;
    fs::create_directories(out);

    // 1) Генерим словарь
    std::vector<std::string> vocab;
    vocab.reserve((size_t)a.vocab);
    for (int i = 0; i < a.vocab; i++) {
        vocab.push_back(rand_word(rng, a.min_word_len, a.max_word_len));
    }

    // 2) Распределение частот: вес ~ 1/(rank^skew)
    //    discrete_distribution принимает веса double.
    std::vector<double> weights;
    weights.reserve((size_t)a.vocab);
    for (int i = 0; i < a.vocab; i++) {
        double rank = double(i + 1);
        double w = 1.0 / std::pow(rank, std::max(0.0, a.skew));
        weights.push_back(w);
    }
    std::discrete_distribution<int> pick_word(weights.begin(), weights.end());

    // 3) Параметры "лог-строки"
    std::uniform_int_distribution<int> msg_words(6, 18);     // слов в сообщении
    std::uniform_int_distribution<int> maybe_comma(0, 99);   // иногда вставлять пунктуацию
    std::uniform_int_distribution<int> maybe_path(0, 99);    // иногда вставлять path-like токен
    std::uniform_int_distribution<int> code_dist(100, 599);  // http-like codes
    std::uniform_int_distribution<int> user_id(1, 2000000);

    auto make_path_token = [&](std::mt19937_64& r) -> std::string {
        // /api/v1/<word>/<word>?id=123
        std::ostringstream oss;
        oss << "/api/v1/";
        oss << vocab[(size_t)pick_word(r)] << "/";
        oss << vocab[(size_t)pick_word(r)] << "?id=" << user_id(r);
        return oss.str();
    };

    const uint64_t bytes_target_per_file = uint64_t(a.mib_per_file) * 1024ull * 1024ull;

    std::cout << "Generating into: " << out.string() << "\n"
              << "Seed: " << seed << "\n"
              << "Files: " << a.files << ", ~" << a.mib_per_file << " MiB each\n"
              << "Vocab: " << a.vocab << ", Skew: " << a.skew << "\n";

    // 4) Генерим файлы
    for (int fi = 0; fi < a.files; fi++) {
        std::ostringstream fname;
        fname << "log_" << std::setw(4) << std::setfill('0') << fi << ".txt";
        fs::path path = out / fname.str();

        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) {
            std::cerr << "Failed to open: " << path.string() << "\n";
            return 1;
        }

        // Крупный буфер для ускорения записи
        std::string buffer;
        buffer.reserve(1 << 20); // ~1MB

        uint64_t written = 0;
        uint64_t base_ts = 1700000000ull + uint64_t(fi) * 12345ull; // псевдо-epoch

        while (written < bytes_target_per_file) {
            // Таймстемп + уровень + ip + код
            uint64_t ts = base_ts + (written / 200); // слегка растёт
            buffer += std::to_string(ts);
            buffer += rand_punct(rng);
            buffer += rand_level(rng);
            buffer += rand_punct(rng);
            buffer += "ip=" + rand_ip(rng);
            buffer += rand_punct(rng);
            buffer += "code=" + std::to_string(code_dist(rng));
            buffer += rand_punct(rng);

            int wc = msg_words(rng);
            for (int i = 0; i < wc; i++) {
                int idx = pick_word(rng);
                std::string w = maybe_mutate(rng, vocab[(size_t)idx]);
                buffer += w;

                if (maybe_path(rng) < 6) {
                    buffer += rand_punct(rng);
                    buffer += make_path_token(rng);
                }

                // иногда вставим пунктуацию, чтобы токенизация была не тривиальной
                if (i + 1 < wc) {
                    if (maybe_comma(rng) < 12) buffer += ", ";
                    else buffer += " ";
                }
            }

            // добавим user_id и "tag"
            buffer += rand_punct(rng);
            buffer += "user_" + std::to_string(user_id(rng));
            buffer += rand_punct(rng);
            buffer += "[tag_" + std::to_string(user_id(rng) % 1000) + "]";
            buffer += "\n";

            // если буфер разросся — сбросим в файл
            if (buffer.size() >= (1 << 20)) {
                ofs.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                written += buffer.size();
                buffer.clear();
            }
        }

        // Допишем остаток
        if (!buffer.empty()) {
            ofs.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            written += buffer.size();
            buffer.clear();
        }

        ofs.close();

        // подрежем файл точно до цели (чтобы размеры были ровнее)
        // (не обязательно, но удобно)
        try {
            auto sz = fs::file_size(path);
            if (sz > bytes_target_per_file) {
                // Быстро "обрежем": перепишем в новый файл нужное количество
                fs::path tmp = path;
                tmp += ".tmp";

                std::ifstream ifs(path, std::ios::binary);
                std::ofstream tfs(tmp, std::ios::binary);

                std::vector<char> chunk(1 << 20);
                uint64_t remain = bytes_target_per_file;
                while (remain > 0 && ifs) {
                    auto to_read = (size_t)std::min<uint64_t>(chunk.size(), remain);
                    ifs.read(chunk.data(), (std::streamsize)to_read);
                    auto got = (size_t)ifs.gcount();
                    if (got == 0) break;
                    tfs.write(chunk.data(), (std::streamsize)got);
                    remain -= got;
                }
                ifs.close();
                tfs.close();

                fs::remove(path);
                fs::rename(tmp, path);
            }
        } catch (...) {
            // если не получилось — не критично
        }

        std::cout << "  wrote " << path.filename().string()
                  << " (" << (fs::file_size(path) / 1024) << " KiB)\n";
    }

    std::cout << "Done.\n";
    return 0;
}
