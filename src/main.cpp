#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <ranges>
#include <set>
#include <filesystem>


std::vector<std::string> split(const std::string& str, const char delimiter) {
  std::vector<std::string> tokens;
  size_t start{0};
  size_t end{str.find(delimiter)};
  while (end != std::string::npos) {
    tokens.push_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }
  tokens.push_back(str.substr(start));
  return tokens;
}

std::vector<std::filesystem::path> get_system_path() {
  const char* path_env = std::getenv("PATH");
  if (!path_env) return {};
  std::string path_str(path_env);

#ifdef _WIN32
  const char delimiter = ';';
#else 
  const char delimiter = ':';
#endif

  auto path_strings = split(path_str, delimiter);
  std::vector<std::filesystem::path> paths;
  for (const auto& p : path_strings) {
    paths.emplace_back(p);
  }
  return paths;
}

void exit_command(std::vector<std::string>& tokens) {
  // default to successful exit
  if (tokens.size() < 2) {
    exit(0);
  } else if (tokens.size() == 2) {
    std::string exit_code = tokens.back();
    if (exit_code == "1") {
      std::exit(1);
    } else {
      std::exit(0);
    }
  } else {
    std::cerr << "Error code not handled yet\n";
  }
}

void echo_command(std::vector<std::string>& tokens) {
  if (tokens.size() < 2) {
    std::cout << "";
  } else {
    // remove "echo" from tokens
    tokens.erase(tokens.begin());
    for (const auto& token : tokens) {
      std::cout << token << " ";
    }
    std::cout << "\n";
  }
}

bool look_for_file_matches(const std::string& filename,
                           const std::vector<std::filesystem::path>& paths) {
  for (const auto& dir : paths) {
    std::filesystem::path candidate = dir / filename;
    if (std::filesystem::exists(candidate)) {
      std::cout << filename << " is "  << candidate.string() << "\n";
      return true;
    }
  }
  return false;
}

void type_command(std::vector<std::string>& tokens, const std::set<std::string_view>& supported_commands) {
  if (tokens.size() < 2) {
    std::cerr << ": not found";
    return;
  }

  const std::string& query = tokens.back();

  // check builtins
  if (supported_commands.find(tokens.back()) != supported_commands.end()) {
    std::cout << query << " is a shell builtin\n";
    return;
  }

  // check executables in PATH
  auto paths = get_system_path();
  bool found = look_for_file_matches(query, paths);

  // not found anywhere
  if (!found) {
    std::cerr << query << ": not found\n";
  }
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::set<std::string_view> supported_commands{"exit","echo","type"};

  // Read-Eval-Print-Loop (REPL)
  while (true) {
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    auto tokens = split(command, ' ');
    auto fn_name = tokens.front();

    if (supported_commands.find(fn_name) == supported_commands.end()) {
      std::cerr << command << ": not found\n";
    }

    if (fn_name == "exit") {
      exit_command(tokens);
    }

    if (fn_name == "echo") {
      echo_command(tokens);
    }

    if (fn_name == "type") {
      type_command(tokens, supported_commands);
    }

  }
}
