#include "include/tokenizer.h"
#include <cctype>

Tokenizer::Tokenizer(std::string line) : m_source(std::move(line)) {}

std::vector<std::string_view> Tokenizer::tokenize() {
  m_tokens.clear();
  m_token_storage.clear();
  m_error.clear();
  // Prevent reallocation that could invalidate string_views (due to SSO moves)
  m_token_storage.reserve(m_source.size());
  m_tokens.reserve(8);

  // move current token into m_token_storage so views remain valid
  auto flush_token = [&]() {
    if (!m_current.empty()) {
      m_token_storage.push_back(std::move(m_current));
      m_tokens.emplace_back(m_token_storage.back());
      m_current.clear();
    }
  };

  State state = State::Normal;

  for (std::size_t i = 0; i < m_source.size(); ++i) {
    const char ch = m_source[i];

    switch (state) {
    case State::Normal:
      // isspace works for \t, \n, \v, ' ' etc.
      if (std::isspace(static_cast<unsigned char>(ch))) {
        flush_token();
      } else if (ch == 0x27) {
        state = State::InSingleQuotes;
      } else if (ch == '"') {
        state = State::InDoubleQuotes;
      } else if (ch == '\\') {
        state = State::Escape;
        m_escape_from = State::Normal;
      } else {
        m_current.push_back(ch);
      }
      break;

    case State::InSingleQuotes:
      if (ch == 0x27) {
        state = State::Normal;
      } else {
        m_current.push_back(ch);
      }
      break;

    case State::InDoubleQuotes:
      if (ch == '"') {
        state = State::Normal;
      } else if (ch == '\\') {
        state = State::Escape;
        m_escape_from = State::InDoubleQuotes;
      } else {
        m_current.push_back(ch);
      }
      break;

    case State::Escape:
      m_current.push_back(ch);
      state = m_escape_from;
      break;
    }
  } // end loop over input chars

  // check state after processing input
  switch (state) {
  case State::Normal:
    flush_token();
    break;
  case State::InSingleQuotes:
    m_error = "unterminated single quote";
    break;
  case State::InDoubleQuotes:
    m_error = "unterminated double quote";
    break;
  case State::Escape:
    m_error = "dangling escape at end of input";
    break;
  }
  return m_tokens;
}
