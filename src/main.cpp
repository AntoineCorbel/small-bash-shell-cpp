#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <ranges>
#include <set>
#include <filesystem>
#include <cstring>
#include <charconv>
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>

#include "include/tokenizer.h"

namespace fs = std::filesystem;


struct ProcessInfo {
  bool found = false;
  bool executable = false;
  std::string path{};
};

// Track last child exit status (for potential $?-like behavior later)
static int g_last_status = 0;

// removed legacy split(); command parsing is handled by Tokenizer

bool is_executable(const fs::path& path) {
    if (!fs::exists(path) || fs::is_directory(path))
        return false;

#ifdef _WIN32
    static const std::vector<std::string> exts = {".exe", ".bat", ".cmd", ".com"};
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
#else
    // On POSIX systems, check execute permission bit.
    auto perms = fs::status(path).permissions();
    return (perms & fs::perms::owner_exec) != fs::perms::none ||
           (perms & fs::perms::group_exec) != fs::perms::none ||
           (perms & fs::perms::others_exec) != fs::perms::none;
#endif
}


std::vector<fs::path> get_system_path() {
  const char* path_env = std::getenv("PATH");
  if (!path_env) return {};
  std::string path_str(path_env);

#ifdef _WIN32
  const char delimiter = ';';
#else 
  const char delimiter = ':';
#endif

  std::vector<fs::path> paths;
  // Split PATH manually on the platform delimiter
  std::size_t start = 0;
  while (start <= path_str.size()) {
    std::size_t end = path_str.find(delimiter, start);
    if (end == std::string::npos) end = path_str.size();
    paths.emplace_back(path_str.substr(start, end - start));
    start = end + 1;
  }
  return paths;
}

void exit_command(std::vector<std::string>& tokens) {
  if (tokens.size() == 1) {
    std::exit(0);
  }

  if (tokens.size() > 2) {
    std::cerr << "exit: too many arguments\n";
    return;
  }

  const std::string& arg = tokens[1];
  int code = 0;
  const char* first = arg.data();
  const char* last = first + arg.size();
  auto res = std::from_chars(first, last, code, 10);
  if (res.ec != std::errc() || res.ptr != last) {
    std::cerr << "exit: numeric argument required\n";
    return;
  }
  std::exit(code);
}

void echo_command(const std::vector<std::string>& tokens) {
  if (tokens.size() < 2) {
    std::cout << "\n";
    return;
  }
  for (std::size_t i = 1; i < tokens.size(); ++i) {
    if (i > 1) std::cout << ' ';
    std::cout << tokens[i];
  }
  std::cout << "\n";
  return;
}

ProcessInfo
look_for_file_matches(const std::string& filename,
                      const std::vector<fs::path>& paths) {

  ProcessInfo process;
  for (const auto& dir : paths) {
    fs::path candidate = dir / filename;
    if (is_executable(candidate)) {
      // std::cout << filename << " is " << candidate.string() << "\n";
      process.found = true;
      process.executable = true;
      process.path = candidate.string();
      return process;
    }
#ifdef _WIN32
    fs::path exe_candidate = candidate;
    exe_candidate += ".exe";
    if (is_executable(exe_candidate)) {
      std::cout << filename << " is " << exe_candidate.string() << "\n";
      process.found = true;
      process.path = exe_candidate.string();
      return process;
    }
#endif
  }
  return process;
}


void type_command(std::vector<std::string>& tokens, const std::set<std::string_view>& supported_commands) {
  if (tokens.size() < 2) {
    std::cerr << ": not found\n";
    return;
  }

  const std::string& query = tokens[1];

  // check builtins
  if (supported_commands.find(query) != supported_commands.end()) {
    std::cout << query << " is a shell builtin\n";
    return;
  }

  // check executables in PATH
  auto paths = get_system_path();
  auto query_process_info = look_for_file_matches(query, paths);

  if (!query_process_info.found) {
    std::cerr << query << ": not found\n";
    return;
  }

  if (query_process_info.executable == true) {
    std::cout << query << " is " << query_process_info.path << "\n";
    return;
  }
}

void non_builtin_command(std::vector<std::string>& tokens) {
  auto paths = get_system_path();
  if (paths.empty()) {
    return;
  }
  auto process_name = tokens.front(); 
  auto process = look_for_file_matches(process_name, paths);
  if (!process.found) {
    std::cerr << process_name << ": not found\n";
    return;
  }
  // process was found, exec() it
  // need the path to be returned too
  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "Fork failed\n";
  }
  if (pid == 0) {
    // child process
    // std::cout << "child process, pid " << getpid() << "\n";
    std::vector<char *> args;
    for (const auto& arg : tokens) {
      args.push_back((char*)arg.c_str());
    }
    args.push_back(nullptr);
    execvp(process_name.c_str(), args.data()); // execvp looks for the process in PATH
    // below is only executed if execvp fails
    std::perror("execv");
    std::exit(127);
  } else {
    // parent process after child terminated
    // std::cout << "parent process, pid " << getpid() << "\n";
    int status = 0;
    pid_t w = -1;
    do {
      w = waitpid(pid, &status, 0);
    } while (w == -1 && errno == EINTR);

    if (w == pid) {
      int code = 0;
      if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        code = 128 + WTERMSIG(status); // common shell convention
      } else {
        code = 128;
      }
      g_last_status = code;
    }
  }
  return;
}

void pwd_command() {
  std::cout << fs::current_path().string() << "\n";
}

void cd_command(std::vector<std::string> tokens) {
  // TODO: implement a stack to be able to use 'cd -' command
  std::string previous_dir = fs::current_path().string();
  // move to home if no args
  if (tokens.size() == 1) {
    char* const home_dir = std::getenv("HOME");
    fs::current_path(home_dir);
    return;
  }

  std::string next_dir = tokens.at(1);
  // tilde leads to home dir
  if (next_dir == "~" && fs::current_path().string() != std::getenv("HOME")) {
    fs::current_path(std::getenv("HOME"));
    return;
  }

  // general case: parse args
  if (!fs::is_directory(next_dir)) {
    std::cerr << "cd: " << next_dir << ": No such file or directory\n"; 
    return;
  }

  fs::current_path(tokens.at(1));
  return;
}

static const std::set<std::string_view> supported_commands{"exit", "echo",
                                                               "type","pwd","cd"};

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;


  // Read-Eval-Print-Loop (REPL)
  while (true) {
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);

    // Use the Tokenizer to parse the input line (supports quotes/escapes)
    Tokenizer tz(std::move(command));
    auto views = tz.tokenize();
    if (tz.has_error()) {
      std::cerr << tz.error_message() << "\n";
      continue;
    }
    if (views.empty()) {
      continue; // ignore blank lines
    }

    // Convert to std::string to keep existing function signatures intact
    std::vector<std::string> tokens;
    tokens.reserve(views.size());
    for (auto v : views) {
      tokens.emplace_back(v);
    }
    std::string fn_name = tokens.front();

    // if (supported_commands.find(fn_name) == supported_commands.end()) {
    //   std::cerr << command << ": not found\n";
    // }

    if (fn_name == "exit") {
      exit_command(tokens);
    }

    if (fn_name == "echo") {
      echo_command(tokens);
    }

    if (fn_name == "type") {
      type_command(tokens, supported_commands);
    }

    if (fn_name == "pwd") {
      pwd_command();
    }

    if (fn_name == "cd") {
      cd_command(tokens);
    }

    // look for the file in PATH and execute if possible
    if (supported_commands.find(fn_name) == supported_commands.end() && !fn_name.empty()) {
      non_builtin_command(tokens);
    }
  }
}
