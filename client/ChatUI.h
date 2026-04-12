#ifndef CHATUI_H
#define CHATUI_H

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <ncurses.h>

struct ChatMessage {
  std::string username;
  std::string text;
  bool isLocal; // true = sent by us (left-aligned), false = received (right-aligned)
};


class MessageQueue {
public:
  void push(ChatMessage msg);
  void drainInto(std::vector<ChatMessage>& out);

private:
  std::deque<ChatMessage> pending_;
  std::mutex mutex_;
};

class ChatUI {
public:
  explicit ChatUI(const std::string& localUsername);
  ~ChatUI();

  ChatUI(const ChatUI&) = delete;
  ChatUI& operator=(const ChatUI&) = delete;

  void addMessage(ChatMessage msg);

  // Safe queue for the network thread to push incoming messages into.
  MessageQueue& incomingQueue() { return incoming_; }

  // Blocking event loop. Returns the typed input string each time Enter is pressed.
  // Returns false when the user types "exit".
  using SendCallback = std::function<void(const std::string&)>;
  void run(SendCallback onSend);

private:
  void initWindows();
  void resizeWindows();
  void redrawChat();
  void redrawInput();

  static std::vector<std::string> wrap(const std::string& text, int width);

  std::string localUsername_;

  WINDOW* chatWin_  = nullptr; 
  WINDOW* inputWin_ = nullptr;

  int rows_ = 0, cols_ = 0; 

  std::vector<ChatMessage> history_;
  std::string inputBuf_;

  MessageQueue incoming_;
};

#endif
