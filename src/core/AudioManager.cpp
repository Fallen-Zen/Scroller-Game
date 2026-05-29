// =============================================================================
// AudioManager.cpp
// =============================================================================

#include "core/AudioManager.h"
#include "core/Logger.h"
#include <stdexcept>
#include <string>
#include <SDL.h>
#include <SDL_mixer.h>

// =============================================================================
// Construction / Destruction
// =============================================================================

AudioManager::AudioManager() {
    // MIX_DEFAULT_FREQUENCY = 44100 Hz — CD quality, universally supported.
    // MIX_DEFAULT_FORMAT    = signed 16-bit samples, system byte order.
    // 2 channels            = stereo output.
    // 2048 chunk size       = audio buffer in samples. Larger = less CPU but
    //                         more latency. 2048 (~46 ms) is a safe default.
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        LOG_ERROR("Mix_OpenAudio failed: %s", Mix_GetError());
        throw std::runtime_error(Mix_GetError());
    }
    LOG_INFO("AudioManager: SDL_mixer initialised (%d Hz, stereo)", MIX_DEFAULT_FREQUENCY);

    // ── Resolve asset base path ───────────────────────────────────────────────
    // SDL_GetBasePath() returns the directory containing the executable.
    // On macOS bundles this is Contents/MacOS/ — but assets live in
    // Contents/Resources/ to avoid Xcode codesign failures. We step up one
    // level and into Resources/ on Apple platforms.
    // On Windows/Linux the assets sit next to the binary, so no adjustment needed.
    char* base = SDL_GetBasePath();
    std::string basePath = base ? std::string(base) : std::string("./");
    SDL_free(base);

#ifdef __APPLE__
    basePath += "../Resources/";
#endif

    // ── Load sound effects ────────────────────────────────────────────────────
    // Missing files produce a warning but don't crash — play() silently skips
    // null chunks so the game remains playable without every sound present.
    this->load(SoundID::Jump,       (basePath + "assets/sounds/jump.wav").c_str());
    this->load(SoundID::Attack,     (basePath + "assets/sounds/attack.wav").c_str());
    this->load(SoundID::PlayerHurt, (basePath + "assets/sounds/player_hurt.wav").c_str());
    this->load(SoundID::EnemyDeath, (basePath + "assets/sounds/enemy_death.wav").c_str());
}

AudioManager::~AudioManager() {
    // Free every loaded chunk in reverse order (good habit, mirrors load order).
    for (int i = N - 1; i >= 0; --i) {
        if (this->m_chunks[i]) {
            Mix_FreeChunk(this->m_chunks[i]);
            this->m_chunks[i] = nullptr;
        }
    }

    // Close the audio device opened in the constructor.
    Mix_CloseAudio();
    LOG_DEBUG("AudioManager: shut down");
}

// =============================================================================
// Playback
// =============================================================================

void AudioManager::play(SoundID id) const {
    Mix_Chunk* chunk = this->m_chunks[static_cast<int>(id)];

    // Silently skip missing sounds — the game should never crash on audio.
    if (!chunk) return;

    // -1 = auto-select the first available channel.
    //  0 = play once (no looping).
    Mix_PlayChannel(-1, chunk, 0);
}

// =============================================================================
// Volume
// =============================================================================

void AudioManager::setVolume(int volume) {
    // Mix_Volume(-1, v) sets volume on ALL channels simultaneously.
    // Volume is clamped to [0, MIX_MAX_VOLUME] (0–128) by SDL_mixer.
    Mix_Volume(-1, volume);
    LOG_DEBUG("AudioManager: volume set to %d", volume);
}

// =============================================================================
// Private helpers
// =============================================================================

void AudioManager::load(SoundID id, const char* path) {
    Mix_Chunk* chunk = Mix_LoadWAV(path);
    if (!chunk) {
        LOG_WARN("AudioManager: could not load '%s' — %s", path, Mix_GetError());
        return;
    }
    this->m_chunks[static_cast<int>(id)] = chunk;
    LOG_DEBUG("AudioManager: loaded '%s'", path);
}
