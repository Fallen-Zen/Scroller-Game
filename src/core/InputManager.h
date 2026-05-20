// =============================================================================
// InputManager.h — Keyboard and gamepad abstraction
//
// Translates raw SDL events into named game Actions so the rest of the engine
// never needs to mention SDL_Keycode or SDL_GameControllerButton. Remapping a
// key later means changing one place (keyToAction) instead of hunting through
// every system that reads input.
//
// Usage pattern (inside the main loop):
//
//   // In processEvents():
//   m_input.handleEvent(e);          // feed each SDL_Event in
//
//   // In update():
//   if (m_input.isHeld(Action::Right))    { ... }  // held this frame
//   if (m_input.isPressed(Action::Jump))  { ... }  // went down this frame only
//   if (m_input.isReleased(Action::Jump)) { ... }  // went up this frame only
//
//   // After render():
//   m_input.endFrame();              // clears justPressed / justReleased
// =============================================================================

#pragma once
#include <SDL.h>
#include <array>

// -----------------------------------------------------------------------------
// Action — all named inputs the game recognises
//
// Add new entries here as features are added (e.g. Interact, Map, Inventory).
// Count must stay last — it is used as the array size sentinel.
// -----------------------------------------------------------------------------
enum class Action {
    Left,
    Right,
    Jump,
    Attack,
    Dash,
    Pause,
    Count   // ← sentinel, not a real action
};

// -----------------------------------------------------------------------------
// InputManager
// -----------------------------------------------------------------------------
class InputManager {
public:
    // Opens the first connected gamepad (if any). Safe to construct even when
    // no gamepad is plugged in — keyboard still works.
    InputManager();

    // Closes the gamepad handle if one was opened.
    ~InputManager();

    // Non-copyable — owns an SDL_GameController* handle.
    InputManager(const InputManager&)            = delete;
    InputManager& operator=(const InputManager&) = delete;

    // ── Event ingestion ───────────────────────────────────────────────────────

    // Feed every SDL_Event from the poll loop into this method.
    // Handles: SDL_KEYDOWN, SDL_KEYUP,
    //          SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP,
    //          SDL_CONTROLLERAXISMOTION (left stick → Left/Right),
    //          SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED.
    void handleEvent(const SDL_Event& e);

    // Call once per frame after SDL_RenderPresent(). Clears the justPressed and
    // justReleased flags so they only fire for one frame each.
    void endFrame();

    // ── Query API ─────────────────────────────────────────────────────────────

    // True every frame the key/button for this action is held down.
    // Use for continuous actions: walking, running, holding a charge.
    bool isHeld(Action a) const;

    // True only on the first frame the key/button was pressed.
    // Use for one-shot actions: jump, attack, dash, menu confirm.
    bool isPressed(Action a) const;

    // True only on the first frame the key/button was released.
    // Use for variable-height jumps (cut vertical velocity on release).
    bool isReleased(Action a) const;

private:
    // Number of actions — drives the fixed-size state array.
    static constexpr int N = static_cast<int>(Action::Count);

    // Per-action state, updated every event.
    struct ActionState {
        bool held         = false; // currently down
        bool justPressed  = false; // went down this frame
        bool justReleased = false; // went up this frame
    };

    std::array<ActionState, N> m_state{};

    // Active gamepad. nullptr when no controller is connected.
    SDL_GameController* m_controller = nullptr;

    // ── Internal helpers ──────────────────────────────────────────────────────

    // Map SDL keyboard symbol → Action index, or -1 if unbound.
    int keyToAction(SDL_Keycode sym) const;

    // Map SDL controller button → Action index, or -1 if unbound.
    int buttonToAction(SDL_GameControllerButton btn) const;

    // Record a press or release event for the given action index.
    void press(int idx);
    void release(int idx);

    // Try to open a gamepad at the given device index.
    void openController(int deviceIndex);

    // Threshold for the left analogue stick to register as Left/Right.
    // SDL axis values range from -32768 to +32767. 8000 ≈ 25% of full travel.
    static constexpr Sint16 AXIS_DEADZONE = 8000;
};
