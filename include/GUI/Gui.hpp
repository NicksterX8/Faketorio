#ifndef GUI_INCLUDED
#define GUI_INCLUDED

#include "My/Vec.hpp"
#include "sdl_gl.hpp"
#include "items/items.hpp"
#include "constants.hpp"
#include "Player.hpp"
#include "commands.hpp"

struct GuiRenderer;

namespace GUI {

class Hotbar {
public:
    SDL_FRect rect; // the rectangle outline of the hotbar, updated every time draw() is called
    My::Vec<SDL_FRect> slots; // the rectangle outlines of the hotbar slots, updated every time draw() is called

    Hotbar() {
        rect = {0, 0, 0, 0};
        slots = My::Vec<SDL_FRect>::Empty();
    }

    SDL_FRect draw(GuiRenderer& renderer, const Player* player, const ItemManager& itemManager);

    void destroy() {
        slots.destroy();
    }
};

struct Console {
    enum class MessageType {
        Default,
        Command,
        CommandResult,
        Error,
        NumTypes
    };

    constexpr static SDL_Color typeColors[(int)MessageType::NumTypes] = {
        {230, 230, 230, 255},
        {220, 220, 60, 255},
        {200, 200, 50, 255},
        {255, 0, 0, 255}
    };

    struct LogMessage {
        std::string text;
        SDL_Color color;
        MessageType type;
        bool playerEntered = false;
    };

    std::vector<LogMessage> log;

    std::string activeMessage;
    int recallIndex = -1; // going through the log, to retype and old message, this index is for seeing what message its on. -1 means not using the log history currently
    int selectedCharIndex = 0;

    #define CONSOLE_LOG_NEW_MESSAGE_OPEN_DURATION 4.0 // seconds
    double timeLastMessageSent = NAN;
    double timeLastCursorMove = NAN;

    bool showLog = false;
    bool promptOpen = false;

    constexpr static SDL_Color activeTextColor = {255, 255, 255, 255};

    bool messageIsActive() const {
        return !activeMessage.empty();
    }

    void pushActiveMessage(MessageType type) {
        if (activeMessage.empty()) return;
        newMessage(activeMessage.c_str(), type);
        activeMessage.clear();

        selectedCharIndex = 0;
        recallIndex = -1;
    }

    void newMessage(const char* text, MessageType type) {
        bool playerEntered = type == MessageType::Default || type == MessageType::Command;
        LogMessage message = {
            .text = text,
            .color = typeColors[(int)type],
            .type = type,
            .playerEntered = playerEntered
        };
        log.push_back(message);

        timeLastMessageSent = Metadata->seconds();
    }

    void moveCursor(int index) {
        int min = MIN(index, activeMessage.size());
        index = MAX(min, -1);
        selectedCharIndex = index;
        timeLastCursorMove = Metadata->seconds();
    }

    int playerMessageCount() const {
        int count = 0;
        for (auto& message : log) {
            count += message.playerEntered;
        }
        return count;
    }

    // will clamp indices below or above the limit to an empty message or oldest message, respectively
    void recallPastMessage() { 
        if (recallIndex >= (int)log.size() - 1) return;
        int relativeIndex = (int)log.size() - recallIndex - 2; // reverse
        relativeIndex = std::clamp(relativeIndex, 0, (int)log.size() - 1);
        for (int i = relativeIndex; i < log.size(); i++) {
            LogMessage newMessage = log[i];
            if (newMessage.type == MessageType::Default) {
                activeMessage = newMessage.text;
                recallIndex += i - relativeIndex + 1;
                break;
            }
        }
        //if (activeMessage.empty()) LogError("Failed to select logged message!");
        //logHistoryIndex = i; // we set the new index to i, not historyIndex because we might not actually end up on that one,
        // if the index is too great (see method desc)
        moveCursor(activeMessage.size());
    }

    // will clamp indices below or above the limit to an empty message or oldest message, respectively
    void recallBackMessage() {
        if (recallIndex <= 0) return; // already on that index, don't need to do anything
        int relativeIndex = (int)log.size() - recallIndex; // reverse
        relativeIndex = std::clamp(relativeIndex, 0, (int)log.size() - 1);
        for (int i = relativeIndex; i >= 0; i--) {
            LogMessage newMessage = log[i];
            if (newMessage.type == MessageType::Default) {
                activeMessage = newMessage.text;
                recallIndex += i - relativeIndex - 1;
                break;
            }
        }
        if (activeMessage.empty()) LogError("Failed to select logged message!");
        //logHistoryIndex = i; // we set the new index to i, not historyIndex because we might not actually end up on that one,
        // if the index is too great (see method desc)
        moveCursor(activeMessage.size());
    }

    void enterChar(char c) {
        int index = MAX(MIN(selectedCharIndex, activeMessage.size()), 0);
        activeMessage.insert(index, 1, c);
        moveCursor(selectedCharIndex + 1);
    }

    void enterText(const char* text) {
        if (!text) return;
        int index = MAX(MIN(selectedCharIndex, activeMessage.size()), 0);
        int textLen = strlen(text);
        activeMessage.insert(index, text);
        moveCursor(selectedCharIndex + textLen);
    }

    CommandInput handleKeypress(SDL_Keycode keycode, ArrayRef<Command> possibleCommands) {
        CommandInput command = {};
        switch (keycode) {
        case SDLK_RETURN2:
        case SDLK_RETURN: {
            recallIndex = -1;

            command = processMessage(activeMessage, possibleCommands);
            if (command) {
                pushActiveMessage(MessageType::Command);
            } else {
                // not command
                pushActiveMessage(MessageType::Default);
            }
            break;
        }
        case SDLK_DELETE:
        case SDLK_BACKSPACE:
            if (!activeMessage.empty()) {
                if (selectedCharIndex > 0) {
                    activeMessage.erase(MIN(selectedCharIndex-1, activeMessage.size()-1), 1);
                    moveCursor(selectedCharIndex - 1);
                }
            }
            break;
        case SDLK_TAB:
            enterChar('\t');
        break;
        case SDLK_UP:
            recallPastMessage();
            break;
        case SDLK_DOWN:
            recallBackMessage();
            break;
        case SDLK_LEFT:
            moveCursor(MAX(selectedCharIndex - 1, 0));
            break;
        case SDLK_RIGHT:
            moveCursor(MIN(selectedCharIndex + 1, activeMessage.size()));
            break;
        } // end keycode switch

        return command;
    }

    // the string buffer must be handled (freed) after calling this function
    My::StringBuffer newLogStringBuffer() const {
        auto buffer = My::StringBuffer::WithCapacity(64);
        for (int i = 0; i < log.size(); i++) {
            buffer += log[i].text.c_str();
        }
        return buffer;
    }

    void destroy() {
        
    }
};


struct Gui {
    My::Vec<SDL_FRect> area = My::Vec<SDL_FRect>(0);
    Hotbar hotbar;
    Console console = {};

    Gui() {}

    void draw(GuiRenderer& renderer, const FRect& viewport, glm::vec2 mousePosition, const Player* player, const ItemManager& itemManager) {
        area.size = 0;
        SDL_FRect hotbarArea = hotbar.draw(renderer, player, itemManager);
        const auto* heldItemStack = &player->heldItemStack;
        if (heldItemStack) {
            const ItemStack* item = heldItemStack->get();
            if (item && !item->empty())
                drawHeldItemStack(renderer, itemManager, *item, mousePosition);
        }
        area.push(hotbarArea);
    }

    void drawHeldItemStack(GuiRenderer& renderer, const ItemManager& itemManager, const ItemStack& itemStack, glm::vec2 pos);

    bool pointInArea(glm::vec2 point) const {
        for (int i = 0; i < area.size; i++) {
            if (point.x > area[i].x && point.x < area[i].x + area[i].w &&
                point.y > area[i].y && point.y < area[i].y + area[i].h) {
                return true;
            }
        }
        return false;
    }

    void destroy() {
        area.destroy();
        hotbar.destroy();
        console.destroy();
    }
};

}

using GUI::Gui;

#endif