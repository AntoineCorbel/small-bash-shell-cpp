#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <ranges>
#include <set>
#include <filesystem>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>


struct ProcessInfo {
  bool found = false;
  bool executable = false;
  std::string path{};
};

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

bool is_executable(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || std::filesystem::is_directory(path))
        return false;

#ifdef _WIN32
    static const std::vector<std::string> exts = {".exe", ".bat", ".cmd", ".com"};
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
#else
    // On POSIX systems, check execute permission bit.
    auto perms = std::filesystem::status(path).permissions();
    return (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ||
           (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none ||
           (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none;
#endif
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

ProcessInfo
look_for_file_matches(const std::string& filename,
                      const std::vector<std::filesystem::path>& paths) {

  ProcessInfo process;
  for (const auto& dir : paths) {
    std::filesystem::path candidate = dir / filename;
    if (is_executable(candidate)) {
      // std::cout << filename << " is " << candidate.string() << "\n";
      process.found = true;
      process.executable = true;
      process.path = candidate.string();
      return process;
    }
#ifdef _WIN32
    std::filesystem::path exe_candidate = candidate;
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
  } else {
    // parent process after child terminated
    // std::cout << "parent process, pid " << getpid() << "\n";
    wait(nullptr);
  }
  return;
}

void pwd_command() {
  std::cout << std::filesystem::current_path().string() << "\n";
}

static const std::set<std::string_view> supported_commands{"exit", "echo",
                                                               "type","pwd"};

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;


  // Read-Eval-Print-Loop (REPL)
  while (true) {
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    auto tokens = split(command, ' ');
    auto fn_name = tokens.front();

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

    // look for the file in PATH and execute if possible
    if (supported_commands.find(fn_name) == supported_commands.end() && !fn_name.empty()) {
      non_builtin_command(tokens);
    }
  }
}
