#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/ui/Notification.hpp>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

using namespace geode::prelude;

namespace {
    struct Song {
        const char* label;
        const char* file;
    };

    constexpr Song BUNDLED[] = {
        {"Neon Runner", "neon-runner.ogg"},
        {"Cube Drift", "cube-drift.ogg"},
        {"Spike Pulse", "spike-pulse.ogg"},
    };
    constexpr size_t BUNDLED_COUNT = sizeof(BUNDLED) / sizeof(BUNDLED[0]);

    void showError(std::string const& text) {
        FLAlertLayer::create("Menu Music Swapper", text, "OK")->show();
    }

    std::optional<std::filesystem::path> findBundled(const char* file) {
        auto mod = Mod::get();
        std::filesystem::path candidates[] = {
            mod->getResourcesDir() / file,
            mod->getResourcesDir() / mod->getID() / file,
            mod->getTempDir() / "resources" / mod->getID() / file,
            mod->getTempDir() / file,
        };
        for (auto const& p : candidates) {
            std::error_code ec;
            if (std::filesystem::exists(p, ec)) return p;
        }
        showError(
            std::string("Could not find ") + file + ". Looked in:\n" +
            mod->getResourcesDir().string()
        );
        return std::nullopt;
    }

    std::optional<std::filesystem::path> pickSong(std::string& label) {
        auto mod = Mod::get();
        auto choice = mod->getSettingValue<std::string>("song");

        if (choice == "Off (default GD music)") {
            return std::nullopt;
        }

        if (choice == "Custom file") {
            auto path = mod->getSettingValue<std::filesystem::path>("custom-song");
            std::error_code ec;
            if (path.empty() || !std::filesystem::exists(path, ec)) {
                showError("Custom song file is not set or was not found.");
                return std::nullopt;
            }
            label = path.filename().string();
            return path;
        }

        if (choice == "Random bundled") {
            auto const& song = BUNDLED[std::rand() % BUNDLED_COUNT];
            label = song.label;
            return findBundled(song.file);
        }

        for (auto const& song : BUNDLED) {
            if (choice == song.label) {
                label = song.label;
                return findBundled(song.file);
            }
        }
        return std::nullopt;
    }
}

// Swap the menu music at the moment the game asks for it, so there is no gap or restart.
class $modify(SwapperAudioEngine, FMODAudioEngine) {
    void playMusic(gd::string path, bool shouldLoop, float fadeInTime, int channel) {
        std::string requested = path;
        if (requested.find("menuLoop") != std::string::npos) {
            std::string label;
            auto song = pickSong(label);
            if (song) {
                FMODAudioEngine::playMusic(song->string(), shouldLoop, fadeInTime, channel);
                return;
            }
        }
        FMODAudioEngine::playMusic(path, shouldLoop, fadeInTime, channel);
    }
};

class $modify(SwapperMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto winSize = CCDirector::get()->getWinSize();

        auto sprite = ButtonSprite::create("Next song");
        sprite->setScale(0.6f);
        auto button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(SwapperMenuLayer::onNextSong)
        );

        auto menu = CCMenu::create();
        menu->setID("next-song-menu"_spr);
        menu->addChild(button);
        menu->setPosition({ 60.f, winSize.height - 24.f });
        this->addChild(menu);

        return true;
    }

    void onNextSong(CCObject*) {
        auto mod = Mod::get();
        auto current = mod->getSettingValue<std::string>("song");

        // Move to the next bundled track. Anything else (random, custom, off) starts at the first.
        size_t next = 0;
        for (size_t i = 0; i < BUNDLED_COUNT; ++i) {
            if (current == BUNDLED[i].label) {
                next = (i + 1) % BUNDLED_COUNT;
                break;
            }
        }
        mod->setSettingValue<std::string>("song", BUNDLED[next].label);

        std::string label;
        auto song = pickSong(label);
        if (!song) return;

        FMODAudioEngine::sharedEngine()->playMusic(song->string(), true, 1.0f, 0);
        Notification::create("Now playing: " + label, NotificationIcon::Success)->show();
    }
};
