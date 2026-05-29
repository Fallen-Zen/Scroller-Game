// =============================================================================
// AudioManager.h — Loads and plays all game sound effects
//
// AudioManager owns every Mix_Chunk* for the lifetime of the game. It follows
// the same pattern as AssetRegistry: a SoundID enum indexes into a fixed-size
// array; the constructor loads everything, the destructor frees it.
//
// Usage:
//   AudioManager audio;
//   audio.play(SoundID::Jump);
//
// SDL_mixer is initialised here and shut down in the destructor. Only one
// AudioManager should exist at a time (owned by Game).
//
// Volume and channel management can be added later — for now every sound plays
// on an auto-selected channel at full volume.
// =============================================================================

#pragma once
#include <SDL_mixer.h>

// ── Sound catalogue ───────────────────────────────────────────────────────────
// Add new entries before Count. The integer value of each entry is its index
// into the m_chunks array — do not reorder or assign explicit values.
enum class SoundID {
    Jump,         // player leaves the ground
    Attack,       // player swings a weapon
    PlayerHurt,   // player takes contact damage
    EnemyDeath,   // enemy HP reaches zero and is removed
    Count         // sentinel — always last, not a real sound
};

class AudioManager {
public:
    // Opens the SDL_mixer device and loads all sound effects from disk.
    // Throws std::runtime_error if SDL_mixer cannot be initialised or if any
    // required sound file is missing.
    AudioManager();

    // Frees all Mix_Chunk* resources and closes the SDL_mixer device.
    ~AudioManager();

    // AudioManager owns SDL_mixer state — copying would double-free chunks.
    AudioManager(const AudioManager&)            = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Play a sound effect once on an auto-selected channel.
    // Silent no-op if the chunk failed to load (avoids crashing on missing files).
    void play(SoundID id) const;

    // Master volume for all sound effects [0, 128]. MIX_MAX_VOLUME = 128.
    void setVolume(int volume);

private:
    // Total number of sound effects — derived from the sentinel enum value.
    static constexpr int N = static_cast<int>(SoundID::Count);

    // One chunk per SoundID. Null if the file failed to load.
    Mix_Chunk* m_chunks[N] = {};

    // Loads a WAV file and stores it at the given SoundID slot.
    // Logs a warning and leaves the slot null on failure — play() handles null.
    void load(SoundID id, const char* path);
};
