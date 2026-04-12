#include "ChatUI.h"

#include <algorithm>
#include <functional>
#include <csignal>

// ---- MessageQueue ----

void MessageQueue::push(ChatMessage msg) {
  std::lock_guard lock(mutex_);
  pending_.push_back(std::move(msg));
}

void MessageQueue::drainInto(std::vector<ChatMessage>& out) {
  std::lock_guard lock(mutex_);
  while (!pending_.empty()) {
    out.push_back(std::move(pending_.front()));
    pending_.pop_front();
  }
}

// ---- helpers -----

// Split text into lines of at most `width` chars, breaking on spaces when possible.
std::vector<std::string> ChatUI::wrap(const std::string& text, int width) {
  std::vector<std::string> lines;
  if (width <= 0) return lines;

  std::size_t pos = 0;
  while (pos < text.size()) {
    // Take up to `width` characters.
    std::size_t len = std::min<std::size_t>(width, text.size() - pos);

    // If there's more text ahead, try to break on the last space.
    if (pos + len < text.size()) {
      std::size_t space = text.rfind(' ', pos + len);
      if (space != std::string::npos && space > pos)
        len = space - pos;
    }

    lines.push_back(text.substr(pos, len));
    pos += len;
    // Skip the breaking space itself.
    if (pos < text.size() && text[pos] == ' ') ++pos;
  }
  return lines;
}

// ---- ChatUI ----

ChatUI::ChatUI(const std::string& localUsername)
  : localUsername_(localUsername)
{
  initscr();
  cbreak();             // characters available immediately
  noecho();             // we draw input ourselves
  keypad(stdscr, TRUE); // enable KEY_RESIZE, KEY_BACKSPACE, etc.
  curs_set(1);

  // Timeout so we can poll incoming_ without blocking forever on getch().
  timeout(50); // ms

  getmaxyx(stdscr, rows_, cols_);
  initWindows();
}

ChatUI::~ChatUI() {
  if (chatWin_)  delwin(chatWin_);
  if (inputWin_) delwin(inputWin_);
  endwin();
}

void ChatUI::initWindows() {
  // Chat window occupies everything but the last 2 rows.
  // Input window is the last 2 rows (separator + prompt line).
  int chatRows = rows_ - 2;

  chatWin_  = newwin(chatRows, cols_, 0, 0);
  inputWin_ = newwin(2, cols_, chatRows, 0);

  scrollok(chatWin_, TRUE); // enable automatic scrolling

  redrawChat();
  redrawInput();
}

void ChatUI::resizeWindows() {
  getmaxyx(stdscr, rows_, cols_);

  delwin(chatWin_);
  delwin(inputWin_);
  chatWin_  = nullptr;
  inputWin_ = nullptr;

  clear();
  refresh();
  initWindows();
}

  // Re-render the entire chat history into chatWin_.
void ChatUI::redrawChat() {
  werase(chatWin_);

  int chatRows, chatCols;
  getmaxyx(chatWin_, chatRows, chatCols);

  // Max bubble width: 60% of terminal, capped at 80 cols.
  const int maxBubble = std::min(80, (int)(chatCols * 0.6));

  // Build a flat list of visual lines to display.
  struct VisualLine { std::string text; bool isLocal; };
  std::vector<VisualLine> vlines;

  for (const auto& msg : history_) {
    // Local messages: "[username]: text". Received messages: already formatted by server.
    std::string full = msg.isLocal ? "[" + msg.username + "]: " + msg.text : msg.text;
    int wrapWidth = std::max(1, maxBubble);

    for (auto& line : wrap(full, wrapWidth))
      vlines.push_back({ line, msg.isLocal });

    // Blank line between messages for visual breathing room.
    vlines.push_back({ "", msg.isLocal });
  }

  // Only show the last chatRows lines.
  int start = std::max(0, (int)vlines.size() - chatRows);
  int row = 0;
  for (int i = start; i < (int)vlines.size(); ++i, ++row) {
    const auto& vl = vlines[i];
    if (vl.isLocal) {
      // Left-aligned (sent by us).
      mvwprintw(chatWin_, row, 0, "%s", vl.text.c_str());
    } else {
      // Right-aligned: anchor to the right edge using the max bubble width,
      // so all lines of a wrapped message share the same right margin.
      int col = chatCols - maxBubble - 1;
      if (col < 0) col = 0;
      mvwprintw(chatWin_, row, col, "%s", vl.text.c_str());
    }
  }

  wrefresh(chatWin_);
}

// Re-render the input area.
void ChatUI::redrawInput() {
  werase(inputWin_);

  // Separator line.
  mvwhline(inputWin_, 0, 0, ACS_HLINE, cols_);

  // Prompt + current input buffer.
  const std::string prompt = "Message: ";
  mvwprintw(inputWin_, 1, 0, "%s%s", prompt.c_str(), inputBuf_.c_str());

  // Park cursor at the end of input.
  wmove(inputWin_, 1, (int)(prompt.size() + inputBuf_.size()));
  wrefresh(inputWin_);
}

void ChatUI::addMessage(ChatMessage msg) {
  history_.push_back(std::move(msg));
}

// Blocking event loop.
void ChatUI::run(SendCallback onSend) {
  while (true) {
    // 1. Drain incoming messages from network thread.
    std::vector<ChatMessage> newMsgs;
    incoming_.drainInto(newMsgs);
    bool needRedraw = !newMsgs.empty();
    for (auto& m : newMsgs)
      history_.push_back(std::move(m));

    if (needRedraw) {
      redrawChat();
      redrawInput(); // restore cursor to input after chat redraws
    }

    // 2. Poll for a keypress (timeout=50ms so we don't busy-spin).
    int ch = getch();
    if (ch == ERR) continue; // timeout, no key

    if (ch == KEY_RESIZE) {
      resizeWindows();
      continue;
    }

    if (ch == '\n' || ch == KEY_ENTER) {
      if (inputBuf_.empty()) continue;

      std::string sent = inputBuf_;
      inputBuf_.clear();

      if (sent == "exit") {
        onSend(sent);
        break;
      }

      // Show our own message left-aligned immediately.
      history_.push_back({ localUsername_, sent, true });
      redrawChat();

      onSend(sent);
      redrawInput();
      continue;
    }

    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (!inputBuf_.empty()) {
        inputBuf_.pop_back();
        redrawInput();
      }
      continue;
    }

    // Printable character.
    if (ch >= 32 && ch < 256) {
      // Guard against overflowing the input line.
      const int maxInput = cols_ - (int)std::string("Message: ").size() - 1;
      if ((int)inputBuf_.size() < maxInput) {
        inputBuf_ += static_cast<char>(ch);
        redrawInput();
      }
    }
  }
}
