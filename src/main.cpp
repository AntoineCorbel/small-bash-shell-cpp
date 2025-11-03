#include <iostream>
#include <string>
#include <vector>
#include <ranges>
#include <set>

std::vector<std::string> split(const std::string& str, char delimiter) {
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

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::set<std::string_view> supported_commands{"exit","echo"};

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

    if (fn_name.contains("exit")) {
      exit_command(tokens);
    }

    if (fn_name.contains("echo")) {
      echo_command(tokens);
    }
  }
}
