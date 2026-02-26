#pragma once

// Shared config keys used by the app UI.
// Stored via KConfig in the user's XDG config dir (e.g. $XDG_CONFIG_HOME).

namespace AppConfigKeys {

constexpr auto kConfigFile = "kdecodexbarrc";

constexpr auto kGeneralGroup = "General";
constexpr auto kRefreshIntervalKey = "refresh_interval";
constexpr auto kAutostartKey = "autostart";

constexpr auto kPlatformsGroup = "Platforms";
constexpr auto kEnableCodexKey = "enable_codex";
constexpr auto kEnableClaudeKey = "enable_claude";
constexpr auto kEnableGeminiKey = "enable_gemini";
constexpr auto kEnableAntigravityKey = "enable_antigravity";
constexpr auto kEnableAmpKey = "enable_amp";

constexpr int kDefaultRefreshIntervalMs = 60000;
constexpr bool kDefaultAutostart = false;
constexpr bool kDefaultPlatformEnabled = true;

} // namespace AppConfigKeys
