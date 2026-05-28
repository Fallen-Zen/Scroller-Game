// =============================================================================
// InputManager.cpp
// =============================================================================

#include "core/InputManager.h"
#include "core/Logger.h"

// =============================================================================
// Construction / Destruction
// =============================================================================

InputManager::InputManager() {
    // Scan for any already-connected controller at startup. SDL_NumJoysticks()
    // returns how many joystick/gamepad devices are plugged in. We open the
    // first one that SDL recognises as a game controller (i.e. has a known
    // button layout). Others can be added later via SDL_CONTROLLERDEVICEADDED.
    int found = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            openController(i);
            found++;
            break; // single-player game — one controller is enough
        }
    }
    if (found == 0)
        LOG_WARN("No gamepad detected — keyboard only");
}

InputManager::~InputManager() {
    // SDL_GameControllerClose releases the handle and its underlying joystick.
    // Forgetting this leaks the device — on some platforms it also prevents
    // other apps from using the controller after the game exits.
    if (m_controller) {
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
    }
}

// =============================================================================
// Event ingestion
// =============================================================================

void InputManager::handleEvent(const SDL_Event& e) {
    switch (e.type) {

    // ── Keyboard ──────────────────────────────────────────────────────────────
    case SDL_KEYDOWN:
        // e.key.repeat is non-zero when the OS fires a key-repeat event after
        // the key has been held for a moment. We ignore repeats so isPressed()
        // fires exactly once per physical key press, not continuously.
        if (e.key.repeat == 0) {
            int idx = keyToAction(e.key.keysym.sym);
            if (idx >= 0) press(idx);
        }
        break;

    case SDL_KEYUP:
        {
            int idx = keyToAction(e.key.keysym.sym);
            if (idx >= 0) release(idx);
        }
        break;

    // ── Gamepad buttons ───────────────────────────────────────────────────────
    case SDL_CONTROLLERBUTTONDOWN:
        {
            auto btn = static_cast<SDL_GameControllerButton>(e.cbutton.button);
            int idx  = buttonToAction(btn);
            if (idx >= 0) press(idx);
        }
        break;

    case SDL_CONTROLLERBUTTONUP:
        {
            auto btn = static_cast<SDL_GameControllerButton>(e.cbutton.button);
            int idx  = buttonToAction(btn);
            if (idx >= 0) release(idx);
        }
        break;

    // ── Gamepad analogue stick ────────────────────────────────────────────────
    // SDL_CONTROLLER_AXIS_LEFTX reports the left stick's horizontal position
    // as a signed 16-bit integer: -32768 = full left, +32767 = full right, 0
    // = centre. We convert it to Left / Right actions by comparing against a
    // deadzone threshold. The deadzone prevents tiny resting-position drift
    // from registering as movement.
    case SDL_CONTROLLERAXISMOTION:
        if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            // Determine what the stick is doing right now.
            bool wantsLeft  = e.caxis.value < -AXIS_DEADZONE;
            bool wantsRight = e.caxis.value >  AXIS_DEADZONE;

            int leftIdx  = static_cast<int>(Action::Left);
            int rightIdx = static_cast<int>(Action::Right);

            // Fire press/release only when the state actually changes.
            // Without this guard we'd call press() every motion event even if
            // the stick was already past the deadzone — spamming justPressed.
            if (wantsLeft  && !m_state[leftIdx].held)  press(leftIdx);
            if (!wantsLeft &&  m_state[leftIdx].held)  release(leftIdx);

            if (wantsRight  && !m_state[rightIdx].held) press(rightIdx);
            if (!wantsRight &&  m_state[rightIdx].held) release(rightIdx);
        }
        break;

    // ── Controller hot-plug ───────────────────────────────────────────────────
    case SDL_CONTROLLERDEVICEADDED:
        // Only connect if we don't already have one. e.cdevice.which is the
        // joystick device index (not the instance ID).
        if (!m_controller)
            openController(e.cdevice.which);
        break;

    case SDL_CONTROLLERDEVICEREMOVED:
        // e.cdevice.which is the instance ID of the removed device.
        // SDL_GameControllerGetJoystick lets us compare against our handle.
        if (m_controller) {
            SDL_Joystick* js = SDL_GameControllerGetJoystick(m_controller);
            if (SDL_JoystickInstanceID(js) == e.cdevice.which) {
                LOG_WARN("Gamepad disconnected — switching to keyboard");
                SDL_GameControllerClose(m_controller);
                m_controller = nullptr;

                // Release any actions that were held via this controller so
                // the game doesn't get stuck with ghost inputs.
                for (int i = 0; i < N; ++i)
                    if (m_state[i].held) release(i);
            }
        }
        break;

    default:
        break;
    }
}

// =============================================================================
// Frame boundary
// =============================================================================

void InputManager::endFrame() {
    // justPressed and justReleased are one-frame-only signals. Clearing them
    // here (after render, before the next processEvents) ensures they are true
    // for exactly one frame no matter how fast the loop runs.
    for (auto& s : m_state) {
        s.justPressed  = false;
        s.justReleased = false;
    }
}

// =============================================================================
// Query API
// =============================================================================

bool InputManager::isHeld(Action a) const {
    return m_state[static_cast<int>(a)].held;
}

bool InputManager::isPressed(Action a) const {
    return m_state[static_cast<int>(a)].justPressed;
}

bool InputManager::isReleased(Action a) const {
    return m_state[static_cast<int>(a)].justReleased;
}

// =============================================================================
// Internal helpers
// =============================================================================

void InputManager::press(int idx) {
    // Guard: if already held, don't overwrite justPressed with a second press.
    // This can happen if two bound keys for the same action are held together.
    if (!m_state[idx].held) {
        m_state[idx].held        = true;
        m_state[idx].justPressed = true;
    }
}

void InputManager::release(int idx) {
    if (m_state[idx].held) {
        m_state[idx].held         = false;
        m_state[idx].justReleased = true;
    }
}

void InputManager::openController(int deviceIndex) {
    m_controller = SDL_GameControllerOpen(deviceIndex);
    if (m_controller)
        LOG_INFO("Gamepad connected: %s", SDL_GameControllerName(m_controller));
    else
        LOG_WARN("SDL_GameControllerOpen(%d) failed: %s", deviceIndex, SDL_GetError());
}

// -----------------------------------------------------------------------------
// Key bindings
//
// Returns the Action index for a given SDL_Keycode, or -1 if unbound.
// To remap, change the cases here — nothing else needs to change.
// -----------------------------------------------------------------------------
int InputManager::keyToAction(SDL_Keycode sym) const {
    switch (sym) {
    case SDLK_LEFT:  case SDLK_a:                      return static_cast<int>(Action::Left);
    case SDLK_RIGHT: case SDLK_d:                      return static_cast<int>(Action::Right);
    case SDLK_SPACE: case SDLK_w: case SDLK_UP:        return static_cast<int>(Action::Jump);
    case SDLK_z:     case SDLK_j:                      return static_cast<int>(Action::Attack);
    case SDLK_x:     case SDLK_k: case SDLK_LSHIFT:    return static_cast<int>(Action::Dash);
    case SDLK_ESCAPE: case SDLK_RETURN:                return static_cast<int>(Action::Pause);
    default:                                           return -1;
    }
}

// -----------------------------------------------------------------------------
// Button bindings (standard gamepad layout)
//
// SDL maps physical buttons to a logical Xbox-style layout regardless of the
// actual controller brand. A = bottom face button, B = right face button, etc.
// -----------------------------------------------------------------------------
int InputManager::buttonToAction(SDL_GameControllerButton btn) const {
    switch (btn) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:              return static_cast<int>(Action::Left);
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:             return static_cast<int>(Action::Right);
    case SDL_CONTROLLER_BUTTON_A:                      return static_cast<int>(Action::Jump);
    case SDL_CONTROLLER_BUTTON_X:                      return static_cast<int>(Action::Attack);
    case SDL_CONTROLLER_BUTTON_B:                      return static_cast<int>(Action::Dash);
    case SDL_CONTROLLER_BUTTON_START:
    case SDL_CONTROLLER_BUTTON_BACK:                   return static_cast<int>(Action::Pause);
    default:                                           return -1;
    }
}
