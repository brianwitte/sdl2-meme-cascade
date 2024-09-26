// SPDX-License-Identifier: MPL-2.0
/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 *  Copyright (c) 2024 Brian Witte
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "SDL_FontCache.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "budget_vectors.h"  // wow, vectors!

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 768
#define TILE_SIZE 32
#define MAP_WIDTH 40
#define MAP_HEIGHT 24
#define TEXT_BOX_COUNT 3
#define TEXT_BOX_SPEED 2
#define TEXT_MIN_SCALE 0.5f
#define TEXT_MAX_SCALE 3.0f
#define TEXT_SCALE_SPEED_MIN 0.002f
#define TEXT_SCALE_SPEED_MAX 0.008f
#define CREDITS_DURATION 15000
#define MEME_SPAWN_INTERVAL 1000
#define IMAGE_LIFETIME 10000
#define IMAGE_MIN_SCALE 0.8f
#define IMAGE_MAX_SCALE 1.2f
#define WIGGLE_IMAGE_SCALE_SPEED 0.2f
#define RICOCHET_DURATION 7000

#define WIGGLE_RADIUS 20
#define WIGGLE_SPEED_X 0.03f
#define WIGGLE_SPEED_Y 0.05f
#define SWIRL_PHASE_OFFSET 1.5f

FC_Font* font = NULL;
SDL_AudioDeviceID audio_device;
bool blackout = false;
bool ricochet_active = false;
bool credits_started = false;
Uint32 ricochet_start_time;
Uint32 credits_start_time;
Uint32 last_meme_spawn_time = 0;
Uint32 game_start_time = 0;
bool scream_played = false;
Uint32 credits_music_start_time = 0;
bool second_scream_triggered = false;

typedef struct {
    vec2 position;
    vec2 velocity;
    float scale;
    float scale_speed;
    const char* text;
} text_box;

typedef struct {
    SDL_Texture* texture;
    vec2 position;
    vec2 velocity;
    float scale;
    Uint32 spawn_time;
} meme_image;

#define MAX_MEMES 10
meme_image memes[MAX_MEMES];
int meme_count = 0;
char* meme_files[200];
int meme_file_count = 0;

// audio function using stb_vorbis
void play_audio(const char* filename, SDL_AudioDeviceID* device) {
    int channels, sample_rate;
    short* output;
    int samples = stb_vorbis_decode_filename(filename, &channels, &sample_rate, &output);
    if (samples <= 0) {
        printf("Failed to load audio: %s\n", filename);
        return;
    }
    SDL_AudioSpec spec;
    spec.freq = sample_rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = channels;
    spec.samples = 4096;
    spec.callback = NULL;

    *device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (*device == 0) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        free(output);
        return;
    }
    SDL_QueueAudio(*device, output, samples * channels * sizeof(short));
    SDL_PauseAudioDevice(*device, 0);  // Start playing
    free(output);
}

void load_meme_files() {
    DIR* dir = opendir("resources/meme_pngs");
    struct dirent* entry;
    meme_file_count = 0;
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".png") != NULL) {
                meme_files[meme_file_count] = malloc(256);
                snprintf(meme_files[meme_file_count], 256, "resources/meme_pngs/%s", entry->d_name);
                meme_file_count++;
            }
        }
        closedir(dir);
    }
}

// Load image using stb_image
SDL_Texture* load_image(SDL_Renderer* renderer, const char* filename, int* width, int* height) {
    int channels;
    unsigned char* image = stbi_load(filename, width, height, &channels, STBI_rgb_alpha);
    if (!image) {
        printf("Failed to load image: %s\n", stbi_failure_reason());
        return NULL;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, *width, *height);
    if (!texture) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        stbi_image_free(image);
        return NULL;
    }
    SDL_UpdateTexture(texture, NULL, image, *width * 4);
    stbi_image_free(image);
    return texture;
}

void spawn_meme(SDL_Renderer* renderer) {
    if (blackout || meme_count >= MAX_MEMES) return;
    // Choose random meme file
    int random_index = rand() % meme_file_count;
    int width, height;
    SDL_Texture* texture = load_image(renderer, meme_files[random_index], &width, &height);
    if (!texture) return;
    // create meme entity
    meme_image meme;
    meme.texture = texture;
    meme.position = vec2_create(rand() % SCREEN_WIDTH, -height);  // start outside of the screen (top)
    meme.velocity = vec2_create(0, 1 + rand() % 3);  // random vertical velocity
    meme.scale = 1.0f;
    meme.spawn_time = SDL_GetTicks();

    // add meme to array
    memes[meme_count++] = meme;
}

void update_and_render_memes(SDL_Renderer* renderer, float time) {
    Uint32 current_time = SDL_GetTicks();
    for (int i = 0; i < meme_count; i++) {
        meme_image* meme = &memes[i];

        // Update position and scale (shrink over time)
        meme->position = vec2_add(meme->position, meme->velocity);
        meme->scale = 1.0f - ((current_time - meme->spawn_time) / (float)IMAGE_LIFETIME);
        if (meme->scale < 0.0f) meme->scale = 0.0f;
        // render meme
        int width, height;
        SDL_QueryTexture(meme->texture, NULL, NULL, &width, &height);
        SDL_Rect dstrect = {
            meme->position.x, meme->position.y,
            (int)(width * meme->scale), (int)(height * meme->scale)
        };
        SDL_RenderCopy(renderer, meme->texture, NULL, &dstrect);

        // remove meme if its lifetime is over or blackout has started
        if (blackout || current_time - meme->spawn_time > IMAGE_LIFETIME) {
            SDL_DestroyTexture(meme->texture);
            meme_count--;
            memes[i] = memes[meme_count];
            i--;
        }
    }
}

void render_wiggling_image(SDL_Renderer* renderer, const char* filename, float time) {
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 4);
    if (!image) {
        printf("Failed to load image: %s\n", stbi_failure_reason());
        return;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        stbi_image_free(image);
        return;
    }
    SDL_UpdateTexture(texture, NULL, image, width * 4);

    // wiggle effect, sorta
    float scale = IMAGE_MIN_SCALE + (IMAGE_MAX_SCALE - IMAGE_MIN_SCALE) * 0.5f *
                  (sin(time * WIGGLE_IMAGE_SCALE_SPEED) + 1.0f);
    int wiggle_x = (int)(sin(time * WIGGLE_SPEED_X) * WIGGLE_RADIUS);
    int wiggle_y = (int)(cos(time * WIGGLE_SPEED_Y + SWIRL_PHASE_OFFSET) * WIGGLE_RADIUS);

    SDL_Rect dstrect = {
        SCREEN_WIDTH / 2 - (int)(width * scale / 2) + wiggle_x,
        SCREEN_HEIGHT / 2 - (int)(height * scale / 2) + wiggle_y,
        (int)(width * scale), (int)(height * scale)
    };

    SDL_RenderCopy(renderer, texture, NULL, &dstrect);
    SDL_DestroyTexture(texture);
    stbi_image_free(image);
}

// Render ricocheting text
void render_ricocheting_texts(SDL_Renderer* renderer, const char* text, text_box* box) {
    box->position = vec2_add(box->position, box->velocity);

    // bounce off the walls
    if (box->position.x < 0 || box->position.x > SCREEN_WIDTH) {
        box->velocity.x = -box->velocity.x;
    }
    if (box->position.y < 0 || box->position.y > SCREEN_HEIGHT) {
        box->velocity.y = -box->velocity.y;
    }
    FC_Draw(font, renderer, box->position.x, box->position.y, text);
}

void render_credits(SDL_Renderer* renderer, Uint32 start_time) {
    Uint32 elapsed = SDL_GetTicks() - start_time;
    float credit_y_offset = SCREEN_HEIGHT - (elapsed / 20.0f);  // Slowly scroll up

    const char* credits[] = {
        "The End",
        "CAST:",
        "Zoomer1: Lit McFire",
        "Zoomer2: Sus Imposter",
        "Main Villain: Dr. Cringe",
        "Technical Advisor: John Carmack",
        "Special Thanks: Other People's Code",
        "Directed by: Chapell Roan",
        "Produced by: Gen Z Studios",
    };

    for (int i = 0; i < sizeof(credits) / sizeof(credits[0]); i++) {
        FC_DrawAlign(
            font,
            renderer,
            SCREEN_WIDTH / 2,
            credit_y_offset + i * 90,
            FC_ALIGN_CENTER,
            credits[i]
            );
    }
    FC_DrawAlign(
        font,
        renderer,
        SCREEN_WIDTH / 2,
        credit_y_offset + (sizeof(credits) / sizeof(credits[0])) * 110,
        FC_ALIGN_CENTER,
        "No Cap"
        );
}


void start_credits() {
    credits_started = true;
    credits_start_time = SDL_GetTicks();
    play_audio("resources/arexs_beat.ogg", &audio_device);  // restart beat for credits
    credits_music_start_time = SDL_GetTicks();  // track when credits music starts
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Meme Cascade", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // initialize SDL_ttf and load font
    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    font = FC_CreateFont();
    FC_LoadFont(font, renderer, "resources/Roboto-Regular.ttf", 64,
        FC_MakeColor(255, 255, 255, 255), TTF_STYLE_NORMAL);                                                              // White text

    play_audio("resources/arexs_beat.ogg", &audio_device);

    load_meme_files();

    // Initialize ricocheting text boxes
    text_box text_boxes[TEXT_BOX_COUNT];
    for (int i = 0; i < TEXT_BOX_COUNT; i++) {
        text_boxes[i].text = "";
        text_boxes[i].position = vec2_create(rand() % SCREEN_WIDTH, rand() % SCREEN_HEIGHT);
        text_boxes[i].velocity = vec2_create((rand() % 2 == 0 ? 1 : -1) * TEXT_BOX_SPEED,
            (rand() % 2 == 0 ? 1 : -1) * TEXT_BOX_SPEED);
        text_boxes[i].scale_speed = TEXT_SCALE_SPEED_MIN + ((float)rand() / RAND_MAX) *
                                    (TEXT_SCALE_SPEED_MAX - TEXT_SCALE_SPEED_MIN);
        text_boxes[i].scale = 1.0f;
    }
    bool quit = false;
    SDL_Event event;
    float time = 0;

    game_start_time = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                quit = true;
            }
        }
        time += 0.02f;

        // trigger meme spawning 3 seconds after game loop starts
        if (!blackout && SDL_GetTicks() - game_start_time > 3000) {
            if (SDL_GetTicks() - last_meme_spawn_time > MEME_SPAWN_INTERVAL) {
                spawn_meme(renderer);
                last_meme_spawn_time = SDL_GetTicks();
            }
        }
        // check if beat finished, start blackout and ricochet screams
        if (SDL_GetQueuedAudioSize(audio_device) == 0 && !blackout) {
            blackout = true;
            play_audio("resources/wilhelm_scream.ogg", &audio_device);
            ricochet_active = true;
            ricochet_start_time = SDL_GetTicks();
        }
        // if ricochet active, stop memes after RICOCHET_DURATION
        if (ricochet_active && SDL_GetTicks() - ricochet_start_time > RICOCHET_DURATION) {
            ricochet_active = false;
            meme_count = 0;  // clear out memes after ricochet ends
            start_credits();
        }
        // trigger second scream 4 seconds after the credits music starts
        if (credits_started && !second_scream_triggered &&
            SDL_GetTicks() - credits_music_start_time > 4000) {
            second_scream_triggered = true;
            play_audio("resources/wilhelm_scream.ogg", &audio_device);
        }
        // clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (!blackout) {
            render_wiggling_image(renderer, "resources/texture.png", time);
        }
        // render and update memes and ricocheting text
        if (ricochet_active) {
            render_ricocheting_texts(renderer, "Oh!", &text_boxes[0]);
            render_ricocheting_texts(renderer, "Arg!", &text_boxes[1]);
            render_ricocheting_texts(renderer, "Oh no!", &text_boxes[2]);
        }
        if (!blackout) {
            update_and_render_memes(renderer, time);
        }
        // start credits if the scream is done
        if (credits_started) {
            render_credits(renderer, credits_start_time);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);  // 60 FPS
    }
    FC_FreeFont(font);
    SDL_CloseAudioDevice(audio_device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
