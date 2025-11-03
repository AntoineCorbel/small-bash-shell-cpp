#include <iostream>
#include <string>
#include <vector>

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

void exit_command(const std::string& command) {
  auto tokens = split(command, ' ');
  std::cout << "Command --> ";
  for (const auto& token : tokens) {
    std::cout << token << " ";
  }
  std::cout << "\n";

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
    throw std::runtime_error("Error code not handled yet.");
  }

}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::vector<std::string_view> supported_commands{"exit"};

  // Read-Eval-Print-Loop (REPL)
  while (true) {
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    if (command.contains("exit")) {
      exit_command(command);
    }
    std::cerr << command << ": command not found\n"; 
  }
}
