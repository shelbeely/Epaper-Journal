#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// PromptPack.h — Daily writing-prompt library
//
// Provides a fixed set of journal writing prompts embedded in flash.
// No heap allocation — all prompts are stored as const string literals
// inside a static function (avoids out-of-class definition in C++11/14).
//
// Usage:
//   struct tm now = gClock.now();
//   const char* prompt = PromptPack::today(now);    // deterministic daily prompt
//   const char* prompt = PromptPack::getPrompt(42); // by index (wraps)
// ─────────────────────────────────────────────────────────────────────────────

#include <stdint.h>
#include <time.h>

struct PromptPack {
    static constexpr uint8_t NUM_PROMPTS = 30;

    // Return the prompt at index `seed % NUM_PROMPTS`.
    static const char* getPrompt(uint32_t seed) {
        // All 30 prompts — each fits on one 800px line at scale 2 (≤ 66 chars).
        static const char* const PROMPTS[NUM_PROMPTS] = {
            "What made you smile today?",
            "Describe a moment you felt grateful for.",
            "What is one thing you want to remember from today?",
            "What challenged you today, and how did you handle it?",
            "What would your future self thank you for today?",
            "Describe the best part of your morning.",
            "Who did you think about today, and why?",
            "What is something you learned today?",
            "How did you take care of yourself today?",
            "What are three things you are grateful for right now?",
            "What do you wish you had done differently today?",
            "Write about something that surprised you.",
            "What is one goal you are working toward this week?",
            "Describe where you are right now in detail.",
            "What emotions came up for you today?",
            "What song, book, or idea has been on your mind lately?",
            "Write a letter to someone you miss.",
            "What does your ideal tomorrow look like?",
            "Describe a conversation that stayed with you.",
            "What is something you are looking forward to?",
            "Write about something you are proud of recently.",
            "What has been weighing on your mind lately?",
            "Describe a place that brings you peace.",
            "What habit would you like to build or break?",
            "Who has positively influenced you recently?",
            "Write about a small moment of joy today.",
            "What would you do if you were not afraid?",
            "How have you grown in the past year?",
            "What kind thing can you do for yourself tomorrow?",
            "What does your heart most want to say right now?",
        };
        return PROMPTS[seed % NUM_PROMPTS];
    }

    // Return the prompt for the given calendar day (rotates through the pack).
    // Uses tm_yday so the same date always yields the same prompt.
    static const char* today(const struct tm& t) {
        // tm_yday is 0-based (0–365); add year offset so prompts don't repeat
        // identically each year without cycling the full pack.
        uint32_t seed = (uint32_t)(t.tm_yday) + (uint32_t)(t.tm_year) * 365u;
        return getPrompt(seed);
    }
};
