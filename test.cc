#include <iostream>
#include <queue>
#include <string>

// g++ -o t test.cc -std=c++17 -ggdb3 -Og

template <typename T>
void clear(T &queue1, T &queue2) {
    while (not queue1.empty()) {
        queue1.pop();
        queue2.pop();
    }
}

class Class1 {
  private:
    std::queue<std::string> _frames_out{};

  public:
    std::queue<std::string> &frames_out() { return _frames_out; }

    void add_s(std::string s) { _frames_out.push(s); }
};

class Class2 {
  public:
    void exchange_frames(Class1 &x, Class1 &y) {
        auto x_frames = x.frames_out(), y_frames = y.frames_out();

        deliver(x_frames, y);
        deliver(y_frames, x);

        clear(x_frames, x.frames_out());
        clear(y_frames, y.frames_out());
    }

    void deliver(const std::queue<std::string> &src, Class1 &dst) {
        std::queue<std::string> to_send = src;
        while (not to_send.empty()) {
            dst.add_s(to_send.front());
            to_send.pop();
        }
    }
};

int main() {
    Class1 c1x, c1y;
    Class2 c2;

    c1x.add_s("1");
    c1x.add_s("2");

    c2.exchange_frames(c1x, c1y);

    return 0;
}