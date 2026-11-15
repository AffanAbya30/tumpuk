#pragma once
#include "game.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace tumpuk {
inline constexpr float screenW = 1280, screenH = 720;
enum class Screen { Menu, Playing, Pause, Result, Credits };

struct Viewport {
    int x, y, w, h;
};

class App {
public:
    App(const std::filesystem::path& creditsFile, const std::filesystem::path& scoreFile);
    void initGraphics();
    void resize(int width, int height);
    void render();
    void update(float dt);
    void key(unsigned char key);
    void pointer(int x, int y);
    void click(int x, int y);
    bool shouldExit() const { return exit_; }
    Screen screen() const { return screen_; }
    const Round& game() const { return round_; }
    int highScore() const { return best_; }

private:
    struct Person { std::string name, nim; };
    enum class Action { Play, Credits, Exit, Resume, Pause, Home, Retry, Previous, Next };
    struct Button { float x, y, w, h; Action action; };
    void action(Action action);
    void start();
    void drop();
    void header();
    void footer();
    void menu();
    void gameplay();
    void modal();
    void credits();
    void button(float x, float y, float w, float h, Action action,
                const std::string& label, bool primary = false, const std::string& hotkey = {});
    void loadCredits();
    void saveBest() const;

    std::filesystem::path creditsFile_, scoreFile_;
    std::vector<Person> people_;
    std::vector<Button> buttons_;
    std::size_t creditsPage_ = 0;
    Screen screen_ = Screen::Menu;
    Round round_;
    int best_ = 0, initialBest_ = 0;
    bool exit_ = false;
    float time_ = 0, mouseX_ = -1, mouseY_ = -1;
    int width_ = 1280, height_ = 720;
    Viewport viewport_{0,0,1280,720};
};

std::filesystem::path scoreLocation(const std::filesystem::path& exeFolder);
}
